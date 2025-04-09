#include "Global.h"
#include "SendDNS.h"

int send_default_dns(uv_loop_t* loop, char* hostname, const char* port, Client* client, dns_query_cb cb) {
    uv_getaddrinfo_t res;
    dns_response_t dns_res = {0};
    dns_res.client_data = client;

    struct addrinfo hints;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    int status = uv_getaddrinfo(loop, &res, NULL, hostname, port, &hints);
    if (status != 0) {
        return -1;
    }

    void* addr_ptr;
    char addr[INET_ADDRSTRLEN];

    struct sockaddr_in* ipv4 = (struct sockaddr_in*)res.addrinfo->ai_addr;
    addr_ptr = &(ipv4->sin_addr);
    inet_ntop(res.addrinfo->ai_family, addr_ptr, addr, sizeof(addr));

    strcpy(dns_res.dns_response.ip_address, addr);
    uv_freeaddrinfo(res.addrinfo);

    dns_res.dns_response.status = 1;
    dns_res.dns_response.hostname = strdup(hostname);
    dns_res.dns_response.port = strdup(port);
    cb(&dns_res);

    free(dns_res.dns_response.hostname);
    free(dns_res.dns_response.port);
    return 1;
}

// DNS 패킷 생성
void create_dns_query(const char* hostname, int query_type, unsigned char* buffer, int* length) {
    // DNS 헤더 (12바이트)
    // ID (16비트)
    buffer[0] = (rand() % 256);
    buffer[1] = (rand() % 256);

    // 플래그 (16비트): 표준 쿼리, 재귀를 원함
    buffer[2] = 0x01;  // RD 비트 활성화
    buffer[3] = 0x00;

    // QDCOUNT: 질문 수 (1)
    buffer[4] = 0x00;
    buffer[5] = 0x01;

    // ANCOUNT, NSCOUNT, ARCOUNT: 모두 0
    buffer[6] = 0x00;
    buffer[7] = 0x00;
    buffer[8] = 0x00;
    buffer[9] = 0x00;
    buffer[10] = 0x00;
    buffer[11] = 0x00;

    // 도메인 이름 인코딩
    int pos = 12;
    const char* domain = hostname;
    const char* label = domain;

    // 각 레이블을 처리 (점으로 구분)
    while (1) {
        const char* dot = strchr(label, '.');
        int label_len;

        if (dot == NULL) {
            label_len = strlen(label);
            buffer[pos++] = label_len;
            memcpy(&buffer[pos], label, label_len);
            pos += label_len;
            break;
        } else {
            label_len = dot - label;
            buffer[pos++] = label_len;
            memcpy(&buffer[pos], label, label_len);
            pos += label_len;
            label = dot + 1;
        }
    }

    // 도메인 이름 종료 (0 길이)
    buffer[pos++] = 0x00;

    // QTYPE (A=1, AAAA=28)
    buffer[pos++] = 0x00;
    buffer[pos++] = query_type;

    // QCLASS (IN=1)
    buffer[pos++] = 0x00;
    buffer[pos++] = 0x01;

    *length = pos;
}

void on_dns_close(uv_handle_t* handle) {
    dns_request_t* req = (dns_request_t*)handle->data;
    if (handle == (uv_handle_t*)&(req->udp_handle.handle)) {
        req->udp_handle.state = -1;
    } else if (handle == (uv_handle_t*)&(req->timeout_timer.timer)) {
        req->timeout_timer.state = -1;
    }

    if (req->udp_handle.state == -1 && req->timeout_timer.state == -1) {
        free(req->res.dns_response.hostname);
        free(req->res.dns_response.port);
        free(req);
    }
}

// UDP 메시지 수신 콜백
void on_udp_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags) {
    dns_request_t* req = (dns_request_t*)handle->data;
    if (nread <= 0 || req == NULL) {
        free(buf->base);
        return;
    }

    // DNS 응답 파싱
    if (nread > 12) {  // 최소 DNS 헤더 크기
        unsigned char* response = (unsigned char*)buf->base;

        // DNS 헤더에서 응답 코드 및 응답 수 얻기
        int qr = (response[2] & 0x80) >> 7;              // QR 비트: 1=응답, 0=질문
        int rcode = response[3] & 0x0F;                  // 응답 코드
        int ancount = (response[6] << 8) | response[7];  // 응답 레코드 수

        if (qr == 1) {  // 응답인 경우
            if (rcode == 0 && ancount > 0) {
                // DNS 응답에서 IP 주소 추출
                unsigned char* ptr = response + 12;  // 헤더(12바이트) 이후부터 시작

                // 질문 섹션 건너뛰기
                while (*ptr != 0) {               // 도메인 이름 끝(0) 찾기
                    if ((*ptr & 0xC0) == 0xC0) {  // 압축된 도메인 이름
                        ptr += 2;                 // 포인터 2바이트 건너뛰기
                        break;
                    }
                    ptr += *ptr + 1;  // 레이블 길이 + 길이 바이트
                }

                if (*ptr == 0) ptr++;  // 도메인 이름 끝(0) 건너뛰기

                // QTYPE과 QCLASS 건너뛰기 (각 2바이트)
                ptr += 4;

                int ip_length = 0;
                int ip_type = 0;
                for (int i = 0; i < ancount; i++) {
                    // 첫 번째 응답 레코드 처리
                    // 이름 필드 건너뛰기 (압축되어 있을 수 있음)
                    if ((ptr[0] & 0xC0) == 0xC0) {
                        ptr += 2;  // 압축된 형식이면 2바이트
                    } else {
                        // 비압축 형식 (7example3com0) 이면 전체 도메인 이름 건너뛰기
                        while (*ptr != 0) ptr += *ptr + 1;
                        ptr++;  // 마지막 0 건너뛰기
                    }

                    // TYPE 필드 (2바이트)
                    int ans_type = (ptr[0] << 8) | ptr[1];
                    ptr += 2;

                    // CLASS 필드 (2바이트)
                    ptr += 2;

                    // TTL 필드 (4바이트)
                    ptr += 4;

                    // RDLENGTH 필드 (2바이트)
                    int rd_length = (ptr[0] << 8) | ptr[1];
                    ptr += 2;

                    // CDN 등으로 인한 다음 CNAME 레코드 처리
                    if (ans_type == 5) {
                        ptr += rd_length;
                    } else {
                        ip_length = rd_length;
                        ip_type = ans_type;
                        break;
                    }
                }

                // RDATA 필드 (IP 주소)
                if (ip_type == 1 && ip_length == 4) {  // A 레코드 (IPv4)
                    sprintf(req->res.dns_response.ip_address, "%d.%d.%d.%d", ptr[0], ptr[1], ptr[2], ptr[3]);
                } else if (ip_type == 28 && ip_length == 16) {  // AAAA 레코드 (IPv6)
                    sprintf(req->res.dns_response.ip_address, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
                            ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15]);
                }
            } else {
                req->res.dns_response.status = -2;
            }

            // 타이머 정지 및 핸들 닫기
            uv_timer_stop(&req->timeout_timer.timer);
            req->cb(&req->res);
            uv_close((uv_handle_t*)&req->udp_handle.handle, on_dns_close);
            uv_close((uv_handle_t*)&req->timeout_timer.timer, on_dns_close);
        }
    }

    free(buf->base);
}

// 타임아웃 콜백
void on_dns_timeout(uv_timer_t* timer) {
    dns_request_t* req = (dns_request_t*)timer->data;

    if (req != NULL) {
        req->res.dns_response.status = -1;
        req->cb(&req->res);
    }

    // 핸들 닫기
    uv_close((uv_handle_t*)&req->udp_handle.handle, on_dns_close);
    uv_close((uv_handle_t*)&req->timeout_timer.timer, on_dns_close);
}

// UDP 전송 콜백
void on_udp_send(uv_udp_send_t* req, int status) { free(req); }

int send_dns_query(uv_loop_t* loop, char* hostname, char* port, char* dns_server, int query_type, uint64_t timeout, Client* client, dns_query_cb cb) {
    struct sockaddr_in dns_addr;
    int r;

    // DNS 서버 주소 설정
    r = uv_ip4_addr(dns_server, 53, &dns_addr);
    if (r) {
        return -1;
    }

    // DNS 요청 객체 초기화
    dns_request_t* req = (dns_request_t*)malloc(sizeof(dns_request_t));
    memset(req, 0, sizeof(dns_request_t));
    req->query_type = query_type;
    req->cb = cb;
    req->res.client_data = client;
    req->res.dns_response.dns_address = dns_server;
    req->res.dns_response.status = 1;
    req->res.dns_response.hostname = strdup(hostname);
    req->res.dns_response.port = strdup(port);
    req->res.dns_response.timeout = timeout;

    // UDP 핸들 초기화
    uv_udp_init(loop, &req->udp_handle.handle);
    uv_timer_init(loop, &req->timeout_timer.timer);

    // 콘텍스트 데이터 설정
    req->udp_handle.handle.data = req;
    req->timeout_timer.timer.data = req;

    // UDP 소켓 바인딩
    struct sockaddr_in any_addr;
    uv_ip4_addr("0.0.0.0", 0, &any_addr);
    r = uv_udp_bind(&req->udp_handle.handle, (const struct sockaddr*)&any_addr, 0);
    if (r) {
        uv_close((uv_handle_t*)&req->udp_handle.handle, on_dns_close);
        uv_close((uv_handle_t*)&req->timeout_timer.timer, on_dns_close);
        return -2;
    }

    // UDP 수신 시작
    r = uv_udp_recv_start(&req->udp_handle.handle, alloc_buffer, on_udp_read);
    if (r) {
        uv_close((uv_handle_t*)&req->udp_handle.handle, on_dns_close);
        uv_close((uv_handle_t*)&req->timeout_timer.timer, on_dns_close);
        return -3;
    }

    // DNS 타임아웃 타이머 시작
    uv_timer_start(&req->timeout_timer.timer, on_dns_timeout, timeout, 0);

    // DNS 쿼리 패킷 생성
    unsigned char dns_packet[512];  // DNS 패킷 최대 크기
    int packet_length;
    create_dns_query(req->res.dns_response.hostname, query_type, dns_packet, &packet_length);

    // 전송 요청 구조체 할당
    uv_udp_send_t* send_req = malloc(sizeof(uv_udp_send_t));

    // 전송 버퍼 설정
    uv_buf_t send_buf = uv_buf_init((char*)dns_packet, packet_length);

    // DNS 쿼리 전송
    r = uv_udp_send(send_req, &req->udp_handle.handle, &send_buf, 1, (const struct sockaddr*)&dns_addr, on_udp_send);
    if (r) {
        free(send_req);
        uv_udp_recv_stop(&req->udp_handle.handle);
        uv_timer_stop(&req->timeout_timer.timer);
        uv_close((uv_handle_t*)&req->udp_handle.handle, on_dns_close);
        uv_close((uv_handle_t*)&req->timeout_timer.timer, on_dns_close);
        return -4;
    }

    return 1;
}