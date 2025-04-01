#include "Global.h"

int SERVER_PORT = 1503;
DnsOptions SERVER_DNS = {NULL};

struct option run_args[] = {{"port", required_argument, 0, 'p'}, {"dns", required_argument, 0, 0}, {"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char *)malloc(suggested_size);
    buf->len = suggested_size;
}

void close_cb(uv_handle_t *handle) {
    Client *client = (Client *)handle->data;

    if (client->proxyClient == (uv_stream_t *)handle) {
        client->proxyClient = NULL;
    } else if (client->targetClient == (uv_stream_t *)handle) {
        client->targetClient = NULL;
    }

    uv_unref(handle);
    free(handle);
    free(client->host);
    handle = NULL;
    client->host = NULL;

    if (client->proxyClient == NULL && client->targetClient == NULL) {
        free(client->target_connecter);
        free(client);
        client = NULL;
    }
}

void on_write(uv_write_t *req, int status) {
    free(req->data);
    free(req);
}

void on_shutdown(uv_shutdown_t *req, int status) { free(req); }