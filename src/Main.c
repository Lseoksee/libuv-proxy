#include "Global.h"
#include "ParseHttp.h"
#include "ParseSNI.h"
#include "PorxyClient.h"
#include "SendDNS.h"
#include "ServerLog.h"
#include "Utills.h"

// 실행인자 DNS용
extern DnsOptions SERVER_DNS;
extern int SERVER_PORT;
extern struct option run_args[];

/** 서버 구동체 */
uv_loop_t *loop;

Client *Create_client() {
    Client *client_data = (Client *)malloc(sizeof(Client));
    memset(client_data, 0, sizeof(Client));
    return client_data;
}

void parse_dns(char *original) {
    char res_buf[INET6_ADDRSTRLEN * 2 + 1];

    // 공백 재거
    int search, res_index = 0;
    for (search = 0; search < strlen(original); search++) {
        if (original[search] != ' ') {
            res_buf[res_index] = original[search];
            res_index++;
        }
    }
    res_buf[res_index] = '\0';

    char *colon = strchr(original, ',');
    if (colon == NULL) {
        SERVER_DNS.dns_1 = strdup(res_buf);
    } else {
        *colon = '\0';
        strchr(colon + 1, ',') ? *strchr(colon + 1, ',') = '\0' : 0;
        SERVER_DNS.dns_1 = strdup(original);
        SERVER_DNS.dns_2 = strdup(colon + 1);
    }
}

void on_dns(dns_response_t *dns) {
    Client *client = dns->client_data;
    dns_response_s res = dns->dns_response;

    // DNS 요청을 완료 했지만 이미 소켓이 닫혀버린 경우 처리
    if (client->proxyClient == NULL || uv_is_closing((uv_handle_t *)client->proxyClient)) {
        unref_client(client);
        return;
    }

    // TODO: client에 메모리 해제시 대응 필요
    if (res.status == 1) {
        // ConnectTargetServer측
        ref_client(client);
        ConnectTargetServer(res.ip_address, atoi(res.port), client);
        put_ip_log(LOG_INFO, client->ClientIP, "%s 접속", res.hostname);
        client->state = 1;
        // dns 요청시 rc 감소용
        unref_client(client);
        return;
    } else {
        // 실행 인자 dns 주소 인경우
        if (SERVER_DNS.dns_1 != NULL) {
            if (res.status == -1) {
                put_ip_log(LOG_WARNING, client->ClientIP, "%s DNS 요청 실패, DNS 주소: %s, 타임아웃", res.hostname, res.dns_address);
            } else if (res.status == -2) {
                put_ip_log(LOG_WARNING, client->ClientIP, "%s DNS 요청 실패, DNS 주소: %s, 해당 호스트를 찾을 수 없음", res.hostname, res.dns_address);
            } else {
                put_ip_log(LOG_WARNING, client->ClientIP, "%s DNS 요청 실패, DNS 주소: %s, 알 수 없는 오류", res.hostname, res.dns_address);
            }

            // 보조 DNS 주소로 다시 시도
            if (res.dns_address == SERVER_DNS.dns_1 && SERVER_DNS.dns_2 != NULL) {
                put_ip_log(LOG_WARNING, client->ClientIP, "%s DNS 주소 %s 로 재시도", res.hostname, SERVER_DNS.dns_2);
                int status = send_dns_query(loop, res.hostname, res.port, SERVER_DNS.dns_2, 1, res.timeout, client, on_dns);
                if (status == 1) {
                    return;
                }
            }
        }
    }

    put_ip_log(LOG_ERROR, client->ClientIP, "%s DNS 서버에서 도메인을 찾을 수 없음, Code: %d", res.hostname, res.status);
}

/** 클라이언트 데이터 읽기 */
void read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    Client *client = (Client *)stream->data;

    if (nread <= 0) {
        if (nread == UV_EOF) {
            put_ip_log(LOG_INFO, client->ClientIP, "클라이언트 연결종료");
        } else {
            put_ip_log(LOG_WARNING, client->ClientIP, "클라이언트 비정상 연결종료: 클라이언트 데이터 읽기 오류, Code: %s", uv_err_name(nread));
        }

        uv_close((uv_handle_t *)stream, close_cb);

        // 프록서 서버 클라이언트 연결 종료 시 타겟 서버에도 연결 종료 요청을 보냄
        if (client->targetClient != NULL) {
            uv_shutdown_t *shutdown_req = (uv_shutdown_t *)malloc(sizeof(uv_shutdown_t));
            uv_shutdown(shutdown_req, client->targetClient, on_shutdown);
        }
        return;
    }

    URL *addr_p = NULL;
    HttpRequest *parseHeader_p = NULL;

    // 최초 연결 시
    if (client->state == 0) {
        URL addr;
        HttpRequest parseHeader;
        dns_response_t dns = {0};

        addr_p = &addr;
        parseHeader_p = &parseHeader;
        char ipaddr[INET6_ADDRSTRLEN];
        char *host, *porxy = NULL;
        int status;

        parseHeader = parse_http_request(buf->base, buf->len);

        if (!parseHeader.state) {
            put_ip_log(LOG_ERROR, client->ClientIP, "허용되지 않는 연결, HTTP 해더 파싱 실패");
            freeHeader(parseHeader_p);
            free(buf->base);
            uv_close((uv_handle_t *)stream, close_cb);
            return;
        }

        for (int i = 0; i < parseHeader.header_count; i++) {
            // 호스트 구하기
            if (strcmp(parseHeader.headers[i].key, "Host") == 0) {
                host = parseHeader.headers[i].value;
            }
            if (strcmp(parseHeader.headers[i].key, "Proxy-Connection") == 0) {
                porxy = parseHeader.headers[i].value;
            }
        }

        if (strcmp(parseHeader.method, "CONNECT") == 0) {
            // HTTPS 연결
            addr = parseURL(parseHeader.url);
            client->connect_mode = PROXY_HTTPS;
        } else {
            // HTTP 연결
            if (host == NULL || porxy == NULL) {
                put_ip_log(LOG_ERROR, client->ClientIP, "프록시 헤더를 찾을 수 없음");
                freeHeader(parseHeader_p);
                free(buf->base);
                uv_close((uv_handle_t *)stream, close_cb);
                return;
            }

            addr = parseURL(host);
            char *send_buf = (char *)malloc(nread);
            memcpy(send_buf, buf->base, nread);
            client->connect_mode = PROXY_HTTP;
            client->send_buf = uv_buf_init(send_buf, nread);
        }

        client->host = strdup(addr.url);

        if (!is_ip(addr.url)) {
            // 기본 DNS 서버 사용
            if (SERVER_DNS.dns_1 == NULL) {
                status = send_default_dns(loop, addr.url, addr.port, client, on_dns);
            }
            // 실행 인자 DNS 서버 사용
            else {
                status = send_dns_query(loop, addr.url, addr.port, SERVER_DNS.dns_1, 1, 5000, client, on_dns);
            }

            if (status != 1) {
                put_ip_log(LOG_ERROR, client->ClientIP, "%s DNS 요청 쿼리 전송 실패, Code: %d", client->host, status);
                uv_close((uv_handle_t *)stream, close_cb);
            } else {
                // DNS 콜백 client 대기를 위한
                ref_client(client);
            }
        } else {
            on_dns(&dns);
        }
    }
    // HTTPS에 경우 Connection Established 패킷을 받은 경우 이후 실제 TLS 패킷을 보냄
    else {
        sendTargetServer(stream, buf->base, nread);
    }

    freeURL(addr_p);
    freeHeader(parseHeader_p);
    free(buf->base);
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        put_time_log(LOG_ERROR, "프록시 연결오류: %s", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, client);
    if (uv_accept(server, (uv_stream_t *)client) == 0) {
        Client *client_data = Create_client();
        client_data->proxyClient = (uv_stream_t *)client;
        ref_client(client_data);
        client->data = client_data;

        get_client_ip((uv_stream_t *)client, client_data->ClientIP, sizeof(client_data->ClientIP));
        uv_read_start((uv_stream_t *)client, alloc_buffer, read_data);
        put_ip_log(LOG_INFO, client_data->ClientIP, "프록시 연결 완료");
    } else {
        put_time_log(LOG_ERROR, "프록시 연결오류: accept 실패");
        uv_close((uv_handle_t *)client, close_cb);
    }
}

void handle_segfault(int sig) { put_time_log(LOG_ERROR, "잘못된 메모리 접근 (Segmentation fault)"); }

int main(int argc, char *argv[]) {
    signal(SIGSEGV, handle_segfault);

    int option_index = 0;
    int opt;

    // 실행 인자 파싱
    while ((opt = getopt_long(argc, argv, "p:h", run_args, &option_index)) != -1) {
        switch (opt) {
            case 0:
                if (strcmp(run_args[option_index].name, "dns") == 0) {
                    parse_dns(optarg);

                    if (!is_ip(SERVER_DNS.dns_1) || !is_ip(SERVER_DNS.dns_2)) {
                        put_log(LOG_ERROR, "잘못된 DNS 주소");
                        return 1;
                    }
                }
                break;
            case 'p':
                SERVER_PORT = atoi(optarg);
                break;
            case 'h':
                print_help();
                return 1;
            default:
                put_log(LOG_INFO, "'%s --help' 로 설명을 확인해 보세요.\n", argv[0]);
                return 0;
        }
    }

    loop = uv_default_loop();

    uv_tcp_t server;
    uv_tcp_init(loop, &server);
    init_PorxyClient(loop);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", SERVER_PORT, &addr);

    uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);
    int r = uv_listen((uv_stream_t *)&server, DEFAULT_BACKLOG, on_new_connection);
    if (r) {
        put_log(LOG_ERROR, "%d 포트가 사용중인거 같음", SERVER_PORT);
        return 1;
    }

    put_log(LOG_INFO, "프록시 서버가 정상적으로 열림");
    if (SERVER_DNS.dns_1 == NULL) {
        put_log(LOG_INFO, "DNS: 기본 DNS 서버");
    } else {
        put_log(LOG_INFO, "DNS: %s,%s", SERVER_DNS.dns_1, SERVER_DNS.dns_2 ? SERVER_DNS.dns_2 : "");
    }
    put_log(LOG_INFO, "포트: %d", SERVER_PORT);
    put_log(LOG_INFO, "");

    return uv_run(loop, UV_RUN_DEFAULT);
}