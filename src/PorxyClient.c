#include "Global.h"

#include "PorxyClient.h"
#include "ServerLog.h"

uv_loop_t *mainLoop = NULL;
extern ServerConfig SERVER_CONFIG;

/** HTTPS 용 타겟 서버 연결 완료시 보내는 패킷 */
const char *established = "HTTP/1.1 200 Connection Established\r\n\r\n";

void init_PorxyClient(uv_loop_t *loop) { mainLoop = loop; }

// 타겟 서버에 데이터 읽기
void read_data_porxy(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    Client *client = (Client *)stream->data;
    if (SERVER_CONFIG.timeOut != 0) {
        uv_timer_again(&client->timeout_timer);
    }

    if (nread <= 0) {
        if (nread == UV_EOF) {
            put_ip_log(LOG_INFO, client->ClientIP, "%s 서버측 연결종료", client->host);
        } else {
            put_ip_log(LOG_WARNING, client->ClientIP,
                       "%s 서버측 비정상 연결종료: 클라이언트 데이터 읽기 오류, Code: %s", client->host,
                       uv_err_name(nread));
        }

        // nreadr가 0인 상태에서도 alloc_buffer는 기본적으로 64KB 수준의 버퍼를 할당하기 때문에 free 안하면 누수 발생
        free(buf->base);
        uv_close((uv_handle_t *)stream, close_cb);

        // 타겟 서버 연결 종료 시 클라이언트에게도 연결 종료 요청을 보냄
        if (client->proxyClient.data != NULL && !uv_is_closing((uv_handle_t *)&client->proxyClient)) {
            uv_close((uv_handle_t *)&client->proxyClient, close_cb);
        }
        return;
    } else if (client->proxyClient.data == NULL || uv_is_closing((uv_handle_t *)&client->proxyClient)) {
        free(buf->base);
        return;
    }

    // INFO:l uv_buf_init안하면 실제 보낸 데이터 범위를 제한 할 수 없기 때문에 쓰래기 값이 들어감
    uv_buf_t resBuffer = uv_buf_init(buf->base, nread);
    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    write_req->data = resBuffer.base;
    int state = uv_write(write_req, (uv_stream_t *)&client->proxyClient, &resBuffer, 1, on_write);
    if (state) {
        put_ip_log(LOG_ERROR, client->ClientIP, "%s 클라이언트 측에 데이터 전송 실패, Code: %s", client->host,
                   uv_strerror(state));
        free(write_req->data);
        free(write_req);
    }
}

// 타겟 서버 연결 완료
void on_connect_porxy(uv_connect_t *req, int status) {
    Client *client = (Client *)req->data;

    if (status < 0) {
        put_ip_log(LOG_WARNING, client->ClientIP, "%s 서버 연결 오류, Code: %s", client->host, uv_strerror(status));
        // on_connect_porxy 에 도달 하기 이전에 proxyClient가 종료된경우 read_data에 nread <= 0 조건 내부 로직으로
        // targetClient도 종료됨경우가 있음 이경우 여기 로직이 실행 되는데 이때 그냥 uv_close() 해버리면 중복 호출이
        // 되버리므로 한번 체크하고 들어가야함
        if (client->targetClient.data != NULL && !uv_is_closing((uv_handle_t *)&client->targetClient)) {
            uv_close((uv_handle_t *)&client->targetClient, close_cb);
        }
        if (client->proxyClient.data != NULL && !uv_is_closing((uv_handle_t *)&client->proxyClient)) {
            uv_close((uv_handle_t *)&client->proxyClient, close_cb);
        }
        return;
    }

    put_ip_log(LOG_INFO, client->ClientIP, "%s 서버 접속", client->host);

    // INFO: 반드시 uv_read_start가 위에 있어야함
    uv_read_start((uv_stream_t *)&client->targetClient, alloc_buffer, read_data_porxy);
    if (client->proxyClient.data == NULL || uv_is_closing((uv_handle_t *)&client->proxyClient)) {
        return;
    }

    uv_buf_t resBuffer;
    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    int state;
    if (client->connect_mode == PROXY_HTTPS) {
        // 성공적으로 대상 서버에 연결을 하면 프록시 서버에 연결된 클라이언트에 Established 응답을 보냄
        // strdup을 쓰는 이유는 on_write_porxy에서 메모리를 free 시키는데 established는 스택 영역에 선언된 상수라서 free
        // 시키면 오류가 발생함
        resBuffer = uv_buf_init(strdup(established), strlen(established));
        write_req->data = resBuffer.base;
        state = uv_write(write_req, (uv_stream_t *)&client->proxyClient, &resBuffer, 1, on_write);
    } else if (client->connect_mode == PROXY_HTTP) {
        resBuffer = client->send_buf;
        write_req->data = resBuffer.base;
        state = uv_write(write_req, (uv_stream_t *)&client->targetClient, &resBuffer, 1, on_write);
    }

    if (state) {
        put_ip_log(LOG_ERROR, client->ClientIP, "%s 서버 측에 데이터 전송 실패, Code: %s", client->host,
                   uv_strerror(state));
        free(write_req->data);
        free(write_req);
    }
}

int ConnectTargetServer(char *addr, int port, Client *client) {
    struct sockaddr_in dest;

    int state;
    state = uv_tcp_init(mainLoop, &client->targetClient);
    state = uv_ip4_addr(addr, port, &dest);
    state = uv_tcp_connect(&client->target_connecter, &client->targetClient, (const struct sockaddr *)&dest,
                           on_connect_porxy);
    if (state) {
        return state;
    }

    client->targetClient.data = client;
    client->target_connecter.data = client;
    return 0;
}

void sendTargetServer(uv_stream_t *clientStream, const char *buf, ssize_t nread) {
    Client *client = (Client *)clientStream->data;
    // INFO: 타겟 서버가 종료가 되었는데, 문제는 비동기 특성으로 인해 sendTargetServer가 먼저 호출되고
    //  uv_is_closing() 함수가 실행 한 도중에 targetClient가 메모리에서 free되면 Segmentation fault 애러가 발생함
    // 지금은 해당현상이 발생할 확률이 현저히 낮지만 만약 추후 발생하면 이 문제를 살펴봐야함
    if (client->targetClient.data == NULL || uv_is_closing((uv_handle_t *)&client->targetClient)) {
        return;
    }

    char *send_buf = (char *)malloc(nread);
    memcpy(send_buf, buf, nread);
    // INFO: uv_buf_init안하면 실제 보낸 데이터 범위를 제한 할 수 없기 때문에 쓰래기 값이 들어감
    uv_buf_t resBuffer = uv_buf_init(send_buf, nread);

    // 프록시 서버에 연결된 클라이언트의 요청을 대상 서버에 전달
    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    write_req->data = resBuffer.base;
    int state = uv_write(write_req, (uv_stream_t *)&client->targetClient, &resBuffer, 1, on_write);
    if (state) {
        put_ip_log(LOG_ERROR, client->ClientIP, "%s 서버 측에 데이터 전송 실패, Code: %s", client->host,
                   uv_strerror(state));
        free(write_req->data);
        free(write_req);
    }
}