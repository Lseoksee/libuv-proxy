#include "Global.h"
#include "ParseHttp.h"
#include "ParseSNI.h"
#include "PorxyClient.h"
#include "Utills.h"

/** 서버 구동체 */
uv_loop_t *loop;

/** 클라이언트 데이터 읽기 */
void read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread <= 0) {
        if (nread != UV_EOF) fprintf(stderr, "read_data, Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t *)stream, close_cb);

        if (stream->data != NULL) {
            Client *client = (Client *)stream->data;

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
    if (!is_connect && stream->data == NULL) {
        fprintf(stderr, "허용되지 않는 연결\n");
        free(buf->base);
        uv_close((uv_handle_t *)stream, close_cb);
        return;
    }

    if (is_connect) {
        Header HostHeader;
        char ipaddr[INET6_ADDRSTRLEN];
        HttpRequest parseHeader = parse_http_request(buf->base, buf->len);

        if (parseHeader.header_count == 0) {
            printf("해더 파싱 실패\n");
            return;
        }

        for (int i = 0; i < parseHeader.header_count; i++) {
            // 호스트 구하기
            if (strcmp(parseHeader.headers[i].key, "Host") == 0) {
                HostHeader = parseHeader.headers[i];
                break;
            }
        }

        URL addr = parseURL(HostHeader.value);

        if (!is_ip(addr.url)) {
            getDnsToAddr(loop, addr.url, addr.port, ipaddr);
        } else {
            strcpy(ipaddr, addr.url);
        }

        ConnectTargetServer(ipaddr, atoi(addr.port), stream);

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
        } */
       sendTargetServer(stream, buf->base, nread);
    }

    free(buf->base);
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        fprintf(stderr, "on_new_connection, 연결오류 %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
    client->data = NULL;
    uv_tcp_init(loop, client);
    if (uv_accept(server, (uv_stream_t *)client) == 0) {
        uv_read_start((uv_stream_t *)client, alloc_buffer, read_data);
    } else {
        uv_close((uv_handle_t *)client, close_cb);
    }
}

void handle_segfault(int sig) {
    fprintf(stderr, "\nERROR: 잘못된 메모리 접근 (Segmentation fault)\n");
    system("pause");
}

int main(int argc, char const *argv[]) {
    signal(SIGSEGV, handle_segfault);

    loop = uv_default_loop();

    uv_tcp_t server;
    uv_tcp_init(loop, &server);
    init_PorxyClient(loop);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", DEFAULT_PORT, &addr);

    uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);
    int r = uv_listen((uv_stream_t *)&server, DEFAULT_BACKLOG, on_new_connection);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }

    return uv_run(loop, UV_RUN_DEFAULT);
}