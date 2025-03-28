#include "Global.h"
#include "ParseHttp.h"
#include "ParseSNI.h"
#include "PorxyClient.h"
#include "SendDNS.h"
#include "ServerLog.h"
#include "Utills.h"

extern char *DNS_SERVER;
extern int SERVER_PORT;
extern struct option run_args[];

/** 서버 구동체 */
uv_loop_t *loop;

void on_dns(dns_response_t *dns) {
    if (dns->status == 1) {
        ConnectTargetServer(dns->ip_address, dns->port, dns->clientStream);
        Client *client = (Client *)dns->clientStream->data;
        client->state = 1;
    }

    if (dns->req != 0) {
        free_dns(dns);
    }
}

/** 클라이언트 데이터 읽기 */
void read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    Client *client = (Client *)stream->data;

    if (nread <= 0) {
        if (nread != UV_EOF) {
            put_ip_log(LOG_WARNING, client->ClientIP, "클라이언트 데이터 읽기 오류, Code: %s", uv_err_name(nread));
        }
        uv_close((uv_handle_t *)stream, close_cb);

        // TODO: 이부분 수정 필요
        if (client != NULL) {
            if (client->targetClient != NULL) {
                uv_shutdown_t *shutdown_req = (uv_shutdown_t *)malloc(sizeof(uv_shutdown_t));
                uv_shutdown(shutdown_req, client->targetClient, on_shutdown);
            }
        }
        return;
    }

    // 프록시 처음 연결인지 확인
    int is_connect = is_connect_request(buf->base);
    // 프록시 연결이 아닌 이상한 패킷 처리
    if (!is_connect && client->state == 0) {
        put_ip_log(LOG_WARNING, client->ClientIP, "허용되지 않는 프로토콜");
        free(buf->base);
        uv_close((uv_handle_t *)stream, close_cb);
        return;
    }

    if (is_connect) {
        Header HostHeader = {NULL};
        char ipaddr[INET6_ADDRSTRLEN];
        HttpRequest parseHeader = parse_http_request(buf->base, buf->len);

        if (parseHeader.header_count == 0) {
            put_ip_log(LOG_WARNING, client->ClientIP, "HTTP 프록시 해더 파싱 실패");
            return;
        }

        for (int i = 0; i < parseHeader.header_count; i++) {
            // 호스트 구하기
            if (strcmp(parseHeader.headers[i].key, "Host") == 0) {
                HostHeader = parseHeader.headers[i];
                break;
            }
        }

        if (HostHeader.value == NULL) {
            put_ip_log(LOG_WARNING, client->ClientIP, "Host 헤더를 찾을 수 없음");
            return;
        }

        URL addr = parseURL(HostHeader.value);

        put_ip_log(LOG_INFO, client->ClientIP, "%s 접속", addr.url);
        client->host = strdup(addr.url);

        int status;
        dns_response_t dns = {0};
        dns.clientStream = stream;
        dns.port = atoi(addr.port);
        dns.hostname = strdup(addr.url);

        if (!is_ip(addr.url)) {
            // 기본 DNS 서버 사용
            if (DNS_SERVER == NULL) {
                status = send_default_dns(loop, addr.url, addr.port, &dns);
                if (status == 1) {
                    on_dns(&dns);
                } else {
                    put_time_log(LOG_ERROR, "DNS 요청 실패 Code: %d\n", status);
                }
            }
            // 실행 인자 DNS 서버 사용
            else {
                status = send_dns_query(loop, addr.url, DNS_SERVER, 1, dns, on_dns);
                if (status != 1) {
                    put_time_log(LOG_ERROR, "DNS 요청 실패 Code: %d", status);
                    free_dns(&dns);
                }
            }
        } else {
            on_dns(&dns);
        }

        freeURL(&addr);
        freeHeader(&parseHeader);
    }
    // 프록시 Respose 이후 클라이언트에 http 연결
    else {
        /*         if (is_clientHello(buf->base, nread)) {
                    resultSNI encryptSNI = encrypt_sni_from_client_hello(buf->base, nread);
                    printf("해싱 이전: %s\n", encryptSNI.beforeSNI);
                    printf("해싱 이후: %s\n", encryptSNI.afterSNI);
                    sendTargetServer(stream, encryptSNI.result_buf, nread);
                    free_resultSNI(&encryptSNI);
                } else {
                    sendTargetServer(stream, buf->base, nread);
                }
        */
        sendTargetServer(stream, buf->base, nread);
    }

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
        Client *client_data = (Client *)malloc(sizeof(Client));
        client_data->state = 0;
        get_client_ip((uv_stream_t *)client, client_data->ClientIP, sizeof(client_data->ClientIP));
        client->data = client_data;
        uv_read_start((uv_stream_t *)client, alloc_buffer, read_data);
        put_ip_log(LOG_INFO, client_data->ClientIP, "프록시 연결 완료");
    } else {
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
                    if (!is_ip(optarg)) {
                        put_log(LOG_ERROR, "잘못된 DNS 주소");
                        return 1;
                    }

                    DNS_SERVER = strdup(optarg);
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
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }

    put_log(LOG_INFO, "프록시 서버가 정상적으로 열림");
    if (DNS_SERVER == NULL) {
        put_log(LOG_INFO, "DNS: 기본 DNS 서버");
    } else {
        put_log(LOG_INFO, "DNS: %s", DNS_SERVER);
    }
    put_log(LOG_INFO, "포트: %d", SERVER_PORT);
    put_log(LOG_INFO, "");

    return uv_run(loop, UV_RUN_DEFAULT);
}