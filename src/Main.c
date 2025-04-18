#include "Global.h"
#include "ParseHttp.h"
#include "ParseSNI.h"
#include "PorxyClient.h"
#include "SendDNS.h"
#include "ServerLog.h"
#include "Utills.h"

// 실행 인자 부분
extern ServerConfig SERVER_CONFIG;

/** 서버 구동체 */
uv_loop_t *loop;

extern struct option run_args[];
extern int dns_timeout;
extern int client_count;

Client *Create_client() {
    Client *client_data = (Client *)malloc(sizeof(Client));
    memset(client_data, 0, sizeof(Client));
    client_count++;
    return client_data;
}

void parse_dns(char *original) {
    // 공백 재거
    removeSpace(original);

    char *colon = strchr(original, ',');
    if (colon == NULL) {
        SERVER_CONFIG.dns.dns_1 = strdup(original);
    } else {
        *colon = '\0';
        strchr(colon + 1, ',') ? *strchr(colon + 1, ',') = '\0' : 0;
        SERVER_CONFIG.dns.dns_1 = strdup(original);
        SERVER_CONFIG.dns.dns_2 = strdup(colon + 1);
    }
}

void on_dns(dns_response_t *dns) {
    Client *client = dns->client_data;
    dns_response_s res = dns->dns_response;

    // DNS 요청을 완료 했지만 이미 소켓이 닫혀버린 경우 처리
    if (client->proxyClient.data == NULL || uv_is_closing((uv_handle_t *)&client->proxyClient)) {
        unref_client(client);
        return;
    }

    if (res.status == 1) {
        // ConnectTargetServer측
        ref_client(client);
        int state = ConnectTargetServer(res.ip_address, atoi(res.port), client);
        if (state) {
            put_ip_log(LOG_ERROR, client->ClientIP, "%s 서버에 연결 실패, Code: %s", client->host, uv_strerror(state));
            uv_close((uv_handle_t *)&client->proxyClient, close_cb);
            unref_client(client);
            return;
        } else {
            client->state = 1;
        }

        // dns 요청시 rc 감소용
        unref_client(client);
        return;
    } else {
        // 실행 인자 dns 주소 인경우
        if (SERVER_CONFIG.dns.dns_1 != NULL) {
            if (res.status == -1) {
                put_ip_log(LOG_WARNING, client->ClientIP, "%s DNS 요청 실패, DNS 주소: %s, 타임아웃", res.hostname,
                           res.dns_address);
            } else if (res.status == -2) {
                put_ip_log(LOG_WARNING, client->ClientIP, "%s DNS 요청 실패, DNS 주소: %s, 해당 호스트를 찾을 수 없음",
                           res.hostname, res.dns_address);
            } else {
                put_ip_log(LOG_WARNING, client->ClientIP, "%s DNS 요청 실패, DNS 주소: %s, 알 수 없는 오류",
                           res.hostname, res.dns_address);
            }

            // 보조 DNS 주소로 다시 시도
            if (res.dns_address == SERVER_CONFIG.dns.dns_1 && SERVER_CONFIG.dns.dns_2 != NULL) {
                put_ip_log(LOG_WARNING, client->ClientIP, "%s DNS 주소 %s 로 재시도", res.hostname,
                           SERVER_CONFIG.dns.dns_2);
                int status = send_dns_query(loop, res.hostname, res.port, SERVER_CONFIG.dns.dns_2, 1, res.timeout,
                                            client, on_dns);
                if (status == 1) {
                    return;
                }
            }
        }
    }

    put_ip_log(LOG_ERROR, client->ClientIP, "%s DNS 서버에서 도메인을 찾을 수 없음, Code: %d", res.hostname,
               res.status);
    uv_close((uv_handle_t *)&client->proxyClient, close_cb);
    unref_client(client);
}

/** 클라이언트 데이터 읽기 */
void read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    Client *client = (Client *)stream->data;

    if (SERVER_CONFIG.timeOut != 0) {
        uv_timer_again(&client->timeout_timer);
    }

    if (nread <= 0) {
        if (nread == UV_EOF) {
            put_ip_log(LOG_INFO, client->ClientIP, "클라이언트 연결종료, Target: %s", client->host);
        } else {
            put_ip_log(LOG_WARNING, client->ClientIP,
                       "클라이언트 비정상 연결종료: 클라이언트 데이터 읽기 오류, Code: %s, Target: %s",
                       uv_err_name(nread), client->host);
        }

        // nreadr가 0인 상태에서도 alloc_buffer는 기본적으로 64KB 수준의 버퍼를 할당하기 때문에 free 안하면 누수 발생
        free(buf->base);
        uv_close((uv_handle_t *)stream, close_cb);

        // 프록서 서버 클라이언트 연결 종료 시 타겟 서버에도 연결 종료 요청을 보냄
        if (client->targetClient.data != NULL && !uv_is_closing((uv_handle_t *)&client->targetClient)) {
            uv_close((uv_handle_t *)&client->targetClient, close_cb);
        }
        return;
    }

    URL *addr_p = NULL;
    HttpRequest *parseHeader_p = NULL;

    // 최초 연결 시
    if (client->state == 0) {
        URL addr = {0};
        HttpRequest parseHeader = {0};
        dns_response_t dns = {0};

        addr_p = &addr;
        parseHeader_p = &parseHeader;
        char ipaddr[INET6_ADDRSTRLEN] = {0};
        char *host, *porxy = NULL;
        int status = 0;

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
                put_ip_log(LOG_ERROR, client->ClientIP, "프록시 헤더를 찾을 수 없음, Target: %s", client->host);
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

        // DNS 콜백 client 대기를 위한
        ref_client(client);
        if (!is_ip(addr.url)) {
            // 기본 DNS 서버 사용
            if (SERVER_CONFIG.dns.dns_1 == NULL) {
                status = send_default_dns(loop, addr.url, addr.port, client, on_dns);
            }
            // 실행 인자 DNS 서버 사용
            else {
                status =
                    send_dns_query(loop, addr.url, addr.port, SERVER_CONFIG.dns.dns_1, 1, dns_timeout, client, on_dns);
            }

            if (status != 1) {
                put_ip_log(LOG_ERROR, client->ClientIP, "%s DNS 요청 쿼리 전송 실패, Code: %d", client->host, status);
                unref_client(client);
                uv_close((uv_handle_t *)stream, close_cb);
            }
        } else {
            int state = ConnectTargetServer(addr.url, atoi(addr.port), client);
            if (state) {
                put_ip_log(LOG_ERROR, client->ClientIP, "%s 서버에 연결 실패, Code: %s", client->host,
                           uv_strerror(state));
                unref_client(client);
            } else {
                client->state = 1;
            }
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

void on_timeout(uv_timer_t *handle) {
    Client *client = (Client *)handle->data;

    put_ip_log(LOG_WARNING, client->ClientIP, "프록시 연결 타임아웃, Target: %s", client->host);

    // INFO: uv_timer_stop 먼저 호출 안하면 uv_timer_again이 호출될 수 있음
    uv_timer_stop(handle);
    if (client->proxyClient.data != NULL && !uv_is_closing((uv_handle_t *)&client->proxyClient)) {
        uv_close((uv_handle_t *)&client->proxyClient, close_cb);
    }
    if (client->targetClient.data != NULL && !uv_is_closing((uv_handle_t *)&client->targetClient)) {
        uv_close((uv_handle_t *)&client->targetClient, close_cb);
    }
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        put_time_log(LOG_ERROR, "프록시 연결오류: %s", uv_strerror(status));
        return;
    }

    Client *client_data = Create_client();
    // proxyClient 소켓이 활성화 됨 ref
    ref_client(client_data);
    uv_tcp_init(loop, &client_data->proxyClient);

    if (uv_accept(server, (uv_stream_t *)&client_data->proxyClient) == 0) {
        get_client_ip((uv_stream_t *)&client_data->proxyClient, client_data->ClientIP, sizeof(client_data->ClientIP));
        // INFO: uv_stream_t의 data는 개발자가 직접 할당 할 수 있다. 이를 이용해서 생성한 client 구조체를 보관한다
        // targetClient.data는 ConnectTargetServer에서 할당
        client_data->proxyClient.data = client_data;

        if (SERVER_CONFIG.timeOut != 0) {
            client_data->timeout_timer.data = client_data;
            uv_timer_init(loop, &client_data->timeout_timer);
            uv_timer_start(&client_data->timeout_timer, on_timeout, SERVER_CONFIG.timeOut, 0);
            // 타임아웃 타이머 작동 ref
            ref_client(client_data);
        }

        int state = uv_read_start((uv_stream_t *)&client_data->proxyClient, alloc_buffer, read_data);
        if (state) {
            put_ip_log(LOG_ERROR, client_data->ClientIP, "프록시 연결 실패, Code: %s", uv_strerror(state));
            uv_close((uv_handle_t *)&client_data->proxyClient, close_cb);
        } else {
            put_ip_log(LOG_INFO, client_data->ClientIP, "프록시 연결 완료");
        }
    } else {
        put_time_log(LOG_ERROR, "프록시 연결오류: accept 실패");
        uv_close((uv_handle_t *)&client_data->proxyClient, close_cb);
    }
}

void handle_segfault(int sig) { put_time_log(LOG_ERROR, "잘못된 메모리 접근 (Segmentation fault)"); }

int main(int argc, char *argv[]) {
    signal(SIGSEGV, handle_segfault);

    // 사용자 입력 모드
    if (argc == 1) {
        char dns[256];
        char port[10];
        int len;

        while (1) {
            printf("포트번호 지정 (default: 1503): \n");
            printf("(기본값을 원한다면 그냥 Enter입력)\n> ");
            fgets(port, sizeof(port), stdin);
            removeSpace(port);

            len = strlen(port);
            port[len - 1] = '\0';

            if (strlen(port) == 0) {
                SERVER_CONFIG.port = 1503;
            } else {
                SERVER_CONFIG.port = atoi(port);
            }

            if (SERVER_CONFIG.port < 1 || SERVER_CONFIG.port > 65535) {
                printf("%s잘못된 포트번호%s\n\n", COLOR_RED, COLOR_RESET);
                continue;
            }

            printf("\n");
            break;
        }

        while (1) {
            printf("DNS 주소 설정 (default: OS 기본 DNS 주소): \n");
            printf("ex) 8.8.8.8,8.8.4.4\n");
            printf("(기본값을 원한다면 그냥 Enter입력)\n> ");
            fgets(dns, sizeof(dns), stdin);
            removeSpace(dns);

            len = strlen(dns);
            dns[len - 1] = '\0';

            if (strlen(dns) > 0) {
                parse_dns(dns);
                if (!is_ip(SERVER_CONFIG.dns.dns_1) ||
                    (SERVER_CONFIG.dns.dns_2 != NULL && !is_ip(SERVER_CONFIG.dns.dns_2))) {
                    printf("%s잘못된 DNS 주소%s\n\n", COLOR_RED, COLOR_RESET);
                    continue;
                }
            }

            printf("\n");
            break;
        }

        // 실행 인자 모드
    } else {
        int option_index = 0;
        int opt;
        while ((opt = getopt_long(argc, argv, "p:ht:l:", run_args, &option_index)) != -1) {
            switch (opt) {
                case 0:
                    if (strcmp(run_args[option_index].name, "dns") == 0) {
                        parse_dns(optarg);
                        if (!is_ip(SERVER_CONFIG.dns.dns_1) ||
                            (SERVER_CONFIG.dns.dns_2 != NULL && !is_ip(SERVER_CONFIG.dns.dns_2))) {
                            put_log(LOG_ERROR, "잘못된 DNS 주소");
                            return 1;
                        }
                    }
                    if (strcmp(run_args[option_index].name, "nolog") == 0) {
                        SERVER_CONFIG.noLog = 1;
                    }
                    break;
                case 'p':
                    if (atoi(optarg) < 1 || atoi(optarg) > 65535) {
                        put_log(LOG_ERROR, "잘못된 포트번호");
                        return 1;
                    }
                    SERVER_CONFIG.port = atoi(optarg);
                    break;
                case 't':
                    if (atoi(optarg) < 0) {
                        put_log(LOG_ERROR, "잘못된 타임아웃 값");
                        return 1;
                    }
                    SERVER_CONFIG.timeOut = atoi(optarg) * 1000;
                    break;
                case 'l':
                    SERVER_CONFIG.logFile = fopen(optarg, "w+");
                    if (SERVER_CONFIG.logFile == NULL) {
                        put_log(LOG_ERROR, "로그 파일을 저장할 수 있는 위치가 아님");
                        return 1;
                    }
                    break;
                case 'h':
                    print_help();
                    return 1;
                default:
                    put_log(LOG_INFO, "'%s --help' 로 설명을 확인해 보세요.", argv[0]);
                    return 0;
            }
        }
    }

    loop = uv_default_loop();

    uv_tcp_t server;
    uv_tcp_init(loop, &server);
    init_PorxyClient(loop);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", SERVER_CONFIG.port, &addr);

    uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);
    int r = uv_listen((uv_stream_t *)&server, DEFAULT_BACKLOG, on_new_connection);
    if (r) {
        put_log(LOG_ERROR_FORCE, "%d 포트가 사용중인거 같음", SERVER_CONFIG.port);
        return 1;
    }

    put_log(LOG_INFO_FORCE, "프록시 서버가 정상적으로 열림");
    if (SERVER_CONFIG.dns.dns_1 == NULL) {
        put_log(LOG_INFO_FORCE, "DNS: 기본 DNS 서버");
    } else {
        put_log(LOG_INFO_FORCE, "DNS: %s,%s", SERVER_CONFIG.dns.dns_1,
                SERVER_CONFIG.dns.dns_2 ? SERVER_CONFIG.dns.dns_2 : "");
    }
    put_log(LOG_INFO_FORCE, "포트: %d", SERVER_CONFIG.port);
    put_log(LOG_INFO_FORCE, "타임아웃 시간: %d초", SERVER_CONFIG.timeOut / 1000);
    put_log(LOG_INFO_FORCE, "");

    return uv_run(loop, UV_RUN_DEFAULT);
}