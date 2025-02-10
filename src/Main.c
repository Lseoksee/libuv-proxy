#include <picohttpparser.h>
#include <stdio.h>
#include <uv.h>

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

/** 클라이언트 데이터 읽기 */
void read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    const char *method = NULL, *path = NULL;
    int pret = 0, minor_version = 0;
    struct phr_header *headers = NULL;
    size_t buflen = 0, prevbuflen = 0, method_len = 0, path_len = 0, num_headers = 0;
    ssize_t rret = 0;

    if (nread > 0) {
        uv_buf_t buffer = uv_buf_init(buf->base, nread);
        int state = phr_parse_request(buffer.base, (size_t)buffer.len, &method, &method_len, &path, &path_len, &minor_version, headers, &num_headers, 0);

        if (state == -1) {
            fprintf(stderr, "요청 파싱 실패\n");
            uv_close((uv_handle_t *)stream, close_cb);
            return;
        }
        printf("%s", path);
    }
    if (nread < 0) {
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