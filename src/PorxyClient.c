#include "Global.h"
#include "PorxyClient.h"
#include "ServerLog.h"

uv_loop_t *mainLoop = NULL;

/** HTTPS 용 타겟 서버 연결 완료시 보내는 패킷 */
const char *established = "HTTP/1.1 200 Connection Established\r\n\r\n";

void init_PorxyClient(uv_loop_t *loop) { mainLoop = loop; }

// 타겟 서버에 데이터 읽기
void read_data_porxy(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    Client *client = (Client *)stream->data;

    if (nread <= 0) {
        if (nread == UV_EOF) {
            put_ip_log(LOG_INFO, client->ClientIP, "%s 서버측 연결종료", client->host);
        } else {
            put_ip_log(LOG_WARNING, client->ClientIP, "%s 서버측 비정상 연결종료: 클라이언트 데이터 읽기 오류, Code: %s", client->host, uv_err_name(nread));
        }

        uv_close((uv_handle_t *)stream, close_cb);

        // 타겟 서버 연결 종료 시 클라이언트에게도 연결 종료 요청을 보냄
        if (client->proxyClient != NULL) {
            uv_shutdown_t *shutdown_req = (uv_shutdown_t *)malloc(sizeof(uv_shutdown_t));
            uv_shutdown(shutdown_req, client->proxyClient, on_shutdown);
        }
        return;
    }

    if (client->proxyClient == NULL || uv_is_closing((uv_handle_t *)client->proxyClient)) {
        return;
    }

    // INFO:l uv_buf_init안하면 실제 보낸 데이터 범위를 제한 할 수 없기 때문에 쓰래기 값이 들어감
    uv_buf_t resBuffer = uv_buf_init(buf->base, nread);
    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    write_req->data = resBuffer.base;
    int state = uv_write(write_req, client->proxyClient, &resBuffer, 1, on_write);
    if (state) {
        put_ip_log(LOG_ERROR, client->ClientIP, "%s 클라이언트 측에 데이터 전송 실패, Code: %s", client->host, uv_strerror(state));
        free(write_req->data);
        free(write_req);
    }
}

// 타겟 서버 연결 완료
void on_connect_porxy(uv_connect_t *req, int status) {
    Client *client = (Client *)req->handle->data;

    if (status < 0) {
        put_ip_log(LOG_WARNING, client->ClientIP, "%s 서버측 연결 오류, Code: %s", client->host, uv_strerror(status));
        uv_close((uv_handle_t *)req->handle, close_cb);
        return;
    }

    if (client->proxyClient == NULL || uv_is_closing((uv_handle_t *)client->proxyClient)) {
        return;
    }

    uv_read_start(client->targetClient, alloc_buffer, read_data_porxy);

    uv_buf_t resBuffer;
    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    int state;
    if (client->connect_mode == PROXY_HTTPS) {
        // 성공적으로 대상 서버에 연결을 하면 프록시 서버에 연결된 클라이언트에 Established 응답을 보냄
        // strdup을 쓰는 이유는 on_write_porxy에서 메모리를 free 시키는데 established는 스택 영역에 선언된 상수라서 free 시키면 오류가 발생함
        resBuffer = uv_buf_init(strdup(established), strlen(established));
        write_req->data = resBuffer.base;
        state = uv_write(write_req, client->proxyClient, &resBuffer, 1, on_write);
    } else if (client->connect_mode == PROXY_HTTP) {
        resBuffer = client->send_buf;
        write_req->data = resBuffer.base;
        state = uv_write(write_req, client->targetClient, &resBuffer, 1, on_write);
    }

    if (state) {
        put_ip_log(LOG_ERROR, client->ClientIP, "%s 서버 측에 데이터 전송 실패, Code: %s", client->host, uv_strerror(state));
        free(write_req->data);
        free(write_req);
    }
}

void sendTargetServer(uv_stream_t *clientStream, const char *buf, ssize_t nread) {
    Client *client = (Client *)clientStream->data;
    // INFO: 타겟 서버가 종료가 되었는데, 문제는 비동기 특성으로 인해 sendTargetServer가 먼저 호출되고
    //  uv_is_closing() 함수가 실행 한 도중에 targetClient가 메모리에서 free되면 Segmentation fault 애러가 발생함
    // 지금은 해당현상이 발생할 확률이 현저히 낮지만 만약 추후 발생하면 이 문제를 살펴봐야함
    if (client->targetClient == NULL || uv_is_closing((uv_handle_t *)client->targetClient)) {
        return;
    }

    char *send_buf = (char *)malloc(nread);
    memcpy(send_buf, buf, nread);
    // INFO:l uv_buf_init안하면 실제 보낸 데이터 범위를 제한 할 수 없기 때문에 쓰래기 값이 들어감
    uv_buf_t resBuffer = uv_buf_init(send_buf, nread);

    // 프록시 서버에 연결된 클라이언트의 요청을 대상 서버에 전달
    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    write_req->data = resBuffer.base;
    int state = uv_write(write_req, client->targetClient, &resBuffer, 1, on_write);
    if (state) {
        put_ip_log(LOG_ERROR, client->ClientIP, "%s 서버 측에 데이터 전송 실패, Code: %s", client->host, uv_strerror(state));
        free(write_req->data);
        free(write_req);
    }
}

int ConnectTargetServer(char *addr, int port, Client *client) {
    struct sockaddr_in dest;

    uv_tcp_t *ClientHandle = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
    uv_connect_t *connecter = (uv_connect_t *)malloc(sizeof(uv_connect_t));

    int state;
    state = uv_tcp_init(mainLoop, ClientHandle);
    state = uv_ip4_addr(addr, port, &dest);
    state = uv_tcp_connect(connecter, ClientHandle, (const struct sockaddr *)&dest, on_connect_porxy);
    if (state) {
        free(ClientHandle);
        free(connecter);
        return state;
    }

    client->target_connecter = connecter;
    client->targetClient = connecter->handle;

    // INFO: uv_stream_t의 data는 개발자가 직접 할당 할 수 있다. 이를 이용해서 생성한 client 구조체를 보관한다
    client->targetClient->data = client;
    return 0;
}