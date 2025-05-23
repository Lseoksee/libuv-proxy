#include "Global.h"
#include "ServerLog.h"

ServerConfig SERVER_CONFIG = {
    .port = 1503,
    .dns = {NULL, NULL},
    .timeOut = 30000,
    .noLog = 0,
    .logFile = NULL,
};

/** DNS 타임아웃 시간 */
int dns_timeout = 5000;
/** 접속 카운트 */
int client_count = 0;

struct option run_args[] = {{"port", required_argument, 0, 'p'},
                            {"dns", required_argument, 0, 0},
                            {"timeout", no_argument, 0, 't'},
                            {"help", no_argument, 0, 'h'},
                            {"nolog", no_argument, 0, 0},
                            {"logfile", required_argument, 0, 'l'},
                            {0, 0, 0, 0}};

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char *)malloc(suggested_size);
    buf->len = suggested_size;
}

void close_cb(uv_handle_t *handle) {
    Client *client = (Client *)handle->data;

    if (&client->proxyClient == (uv_tcp_t *)handle) {
        client->proxyClient.data = NULL;
    } else if (&client->targetClient == (uv_tcp_t *)handle) {
        client->targetClient.data = NULL;
    } else if (&client->timeout_timer == (uv_timer_t *)handle) {
        client->timeout_timer.data = NULL;
    }

    // 프록시 서버와 타겟 서버 모두 정리가 됬는데 타임아웃 상태가 아닌경우에 작동함
    if (client->proxyClient.data == NULL && client->targetClient.data == NULL && client->timeout_timer.data != NULL) {
        uv_timer_stop(&client->timeout_timer);
        uv_close((uv_handle_t *)&client->timeout_timer, close_cb);
    }

    unref_client(client);
}

void on_write(uv_write_t *req, int status) {
    free(req->data);
    free(req);
}

void on_shutdown(uv_shutdown_t *req, int status) { free(req); }

void ref_client(Client *client) { client->ref_count++; }

void unref_client(Client *client) {
    client->ref_count--;
    if (client->ref_count == 0) {
        client_count--;
        put_ip_log(LOG_INFO, client->ClientIP, "최종 통신 종료, Target: %s, 남은 접속 수: %d", client->host,
                   client_count);
        free(client->host);
        free(client);
    }
}