#include "PorxyClient.h"

uv_loop_t *mainLoop = NULL;

/** 타겟 서버 연결 완료시 보내는 패킷 */
const char *established = "HTTP/1.1 200 Connection Established\r\n\r\n";

void init_PorxyClient(uv_loop_t *loop) {
    mainLoop = loop;
}

void read_data_porxy(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    Client *client = (Client *)stream->data;
    if (nread <= 0) {
        if (nread != UV_EOF) fprintf(stderr, "on_connect_porxy, 읽기 오류: %s\n", uv_err_name(nread));
        uv_close((uv_handle_t *)stream, close_cb);

        uv_shutdown_t* shutdown_req = (uv_shutdown_t*) malloc(sizeof(uv_shutdown_t));
        uv_shutdown(shutdown_req, client->proxyClient, on_shutdown);
        return;
    }

    if (uv_is_closing((uv_handle_t *)client->proxyClient)) {
        fprintf(stderr, "read_data_porxy, 소켓이 종료됨\n");
        return;
    }

    printf("read_data_porxy, host: %s\n", client->host);

    // INFO:l uv_buf_init안하면 실제 보낸 데이터 범위를 제한 할 수 없기 때문에 쓰래기 값이 들어감
    uv_buf_t resBuffer = uv_buf_init(buf->base, nread);
    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    write_req->data = resBuffer.base;
    uv_write(write_req, client->proxyClient, &resBuffer, 1, on_write);
}

void on_connect_porxy(uv_connect_t *req, int status) {
    if (status < 0) {
        fprintf(stderr, "on_connect_porxy, 연결오류 %s\n", uv_strerror(status));
        return;
    }

    Client *client = (Client *)req->handle->data;
    if (uv_is_closing((uv_handle_t *)client->proxyClient)) {
        fprintf(stderr, "on_connect_porxy, 소켓이 종료됨\n");
        return;
    }

    uv_read_start(client->targetClient, alloc_buffer, read_data_porxy);

    // 성공적으로 대상 서버에 연결을 하면 프록시 서버에 연결된 클라이언트에 Established 응답을 보냄
    // strdup을 쓰는 이유는 on_write_porxy에서 메모리를 free 시키는데 established는 스택 영역에 선언된 상수라서 free 시키면 오류가 발생함
    uv_buf_t resBuffer = uv_buf_init(strdup(established), strlen(established));
    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    write_req->data = resBuffer.base;

    // INFO: 해당 과정에서 간혈적 오류 발생
    uv_write(write_req, client->proxyClient, &resBuffer, 1, on_write);
}

void sendTargetServer(uv_stream_t *clientStream, const uv_buf_t *buf, ssize_t nread) {
    Client *client = (Client *)clientStream->data;
    if (uv_is_closing((uv_handle_t *)client->targetClient)) {
        fprintf(stderr, "sendTargetServer, 소켓이 종료됨\n");
        return;
    }

    printf("sendTargetServer, host: %s, addr: %p\n", client->host, &client->targetClient);

    // INFO:l uv_buf_init안하면 실제 보낸 데이터 범위를 제한 할 수 없기 때문에 쓰래기 값이 들어감
    uv_buf_t resBuffer = uv_buf_init(buf->base, nread);

    // 프록시 서버에 연결된 클라이언트의 요청을 대상 서버에 전달
    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    write_req->data = resBuffer.base;
    uv_write(write_req, client->targetClient, &resBuffer, 1, on_write);
}

void ConnectTargetServer(char *addr, int port, uv_stream_t *clientStream) {
    struct sockaddr_in dest;
    Client *client = (Client *)malloc(sizeof(Client));
    client->handle = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));

    client->proxyClient = clientStream;
    strcpy(client->host, addr);

    uv_connect_t *connecter = malloc(sizeof(uv_connect_t));

    uv_tcp_init(mainLoop, client->handle);
    uv_ip4_addr(addr, port, &dest);
    uv_tcp_connect(connecter, client->handle, (const struct sockaddr *)&dest, on_connect_porxy);

    client->targetClient = connecter->handle;

    //INFO: uv_stream_t의 data는 개발자가 직접 할당 할 수 있다. 이를 이용해서 생성한 client 구조체를 보관한다
    client->proxyClient->data = client;
    client->targetClient->data = client;
}