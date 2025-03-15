#include "Global.h"
#include "ParseHttp.h"
#include "PorxyClient.h"
#include "Utills.h"

/** 서버 구동체 */
uv_loop_t *loop;

/** 클라이언트 데이터 읽기 */
void read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        HttpRequest req;
        // 프록시 처음 연결 여부용
        int proxyMode = 0;
        char host[1024];
        URL url;

        parse_http_request(buf->base, buf->len, &req);
        for (int i = 0; i < req.header_count; i++) {
            // 호스트 구하기
            if (strcmp(req.headers[i].key, "Host") == 0) {
                strcpy(host, req.headers[i].value);
            }

            if (strcmp(req.headers[i].key, "Proxy-Connection") == 0) {
                proxyMode = 1;
            }
        }

        // 프록시 처음 연결
        if (proxyMode) {
            if (host != NULL) {
                printf("host: %s\n", host);
                // TODO: 단순 IP로 연결할 경우에도 처리해야함
                parseURL(host, &url);
            }

            char *getHost = (&url)->url;
            if (!is_ip((&url)->url)) {
                getHost = getDnsToAddr(loop, (&url)->url, (&url)->port);
            }

            ConnectTargetServer(getHost, atoi((&url)->port), stream);

            free(buf->base);
        }
        // 프록시 Respose 이후 클라이언트에 http 연결
        else {
            sendTargetServer(stream, buf, nread);
        }

    } else {
        if (nread != UV_EOF) fprintf(stderr, "read_data, Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t *)stream, close_cb);
    }
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        fprintf(stderr, "on_new_connection, 연결오류 %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, client);
    if (uv_accept(server, (uv_stream_t *)client) == 0) {
        uv_read_start((uv_stream_t *)client, alloc_buffer, read_data);
    } else {
        uv_close((uv_handle_t *)client, close_cb);
    }
}

int main(int argc, char const *argv[]) {
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