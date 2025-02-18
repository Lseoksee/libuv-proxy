#include <ParseHttp.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

typedef struct {
    int proxyMode;
    int HttpMode;
} ReqestMode;

// 연결 요청 대기 큐 최대길이 (리눅스 기본값 128개)
#define DEFAULT_BACKLOG 128
#define DEFAULT_PORT 80

// 이거 서버에 구동 구조체
uv_loop_t *loop;

/** 연결 종료된 클라이언트는 free */
void close_cb(uv_handle_t *handle) { free(handle); }

/** 버퍼 할당 함수 */
void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char *)malloc(suggested_size);
    buf->len = suggested_size;
}

void on_write(uv_write_t *req, int status) {
    if (status < 0) {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
    }
    printf("Write Data\n");
    free(req->data);
    free(req);
}

/** 클라이언트 데이터 읽기 */
void read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        HttpRequest *req = (HttpRequest *)malloc(sizeof(HttpRequest));
        //TODO: buffer가 callback 타면서 덮어씌워지는 문제가 있음
        uv_buf_t buffer = uv_buf_init(buf->base, nread);
        fprintf(stdout, "Read Data: %s\n", buffer.base);

        ReqestMode mode = {.HttpMode = 1, .proxyMode = 0};
        char *host;

        parse_http_request(buffer.base, buf->len, req);
        for (int i = 0; i < req->header_count; i++) {
            // 호스트 구하기
            if (strcmp(req->headers[i].key, "Host") == 0) {
                host = req->headers[i].value;
            }

            if (strcmp(req->headers[i].key, "Proxy-Connection") == 0) {
                mode.HttpMode = 0;
                mode.proxyMode = 1;
            }
        }

        // 프록시 처음 연결
        if (mode.proxyMode) {
            const char *response = "HTTP/1.1 200 Connection Established\r\n\r\n";
            uv_buf_t resBuffer = uv_buf_init(strdup(response), strlen(response));

            uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
            write_req->data = resBuffer.base;
            uv_write(write_req, stream, &resBuffer, 1, on_write);
            // URL *url = (URL *)malloc(sizeof(URL));
            // parseURL(host, url);
            // printf("Host: %s, Port: %d\n", url->url, url->port);
        }
        // 프록시 Respose 이후 클라이언트에 http 연결
        else if (mode.HttpMode) {
        }

        free_headers(req);
    } else {
        if (nread != UV_EOF) fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t *)stream, close_cb);
    }

    free(buf->base);
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        fprintf(stderr, "연결오류 %s\n", uv_strerror(status));
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