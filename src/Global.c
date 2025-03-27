#include "Global.h"

int SERVER_PORT = 1503;
char *DNS_SERVER = NULL;

struct option run_args[] = {{"port", required_argument, 0, 'p'}, {"dns", required_argument, 0,  0}, {"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char *)malloc(suggested_size);
    buf->len = suggested_size;
}

void close_cb(uv_handle_t *handle) {
    free(handle);
    handle = NULL;
}

void on_write(uv_write_t *req, int status) {
    if (status < 0) {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
    }

    free(req->data);
    free(req);
}

void on_shutdown(uv_shutdown_t *req, int status) { free(req); }