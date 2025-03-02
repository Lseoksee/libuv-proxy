#include <stdio.h>
#include <string.h>
#include <uv.h>

const char *established = "HTTP/1.1 200 Connection Established\r\n\r\n";

typedef struct {
    /** 프록시 서버가 대상 서버에 연결하기 위한 클라이언트 (즉 요청을 위한) */
    uv_connect_t targetClient;
    /** 프록시 서버에 접속하는 클라이언트 (즉 응답을 위한) */
    uv_stream_t *Client;
    uv_tcp_t handle;
    char host[1024];
} ClientList;

uv_loop_t *loop = NULL;
ClientList *clients = NULL;

int stack = 0;
ClientList *add_client(ClientList *client) {
    clients = (ClientList *)realloc(clients, sizeof(ClientList) * (stack + 1));
    clients[stack] = *client;
    stack++;
    return clients;
}

void init_PorxyClient(uv_loop_t *Mainloop) {
    loop = Mainloop;
    clients = (ClientList *)malloc(sizeof(ClientList));
}

/** 연결 종료된 클라이언트는 free */
void close_cb_porxy(uv_handle_t *handle) { 
    return;    
}

void alloc_buffer_porxy(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char *)malloc(suggested_size);
    buf->len = suggested_size;
}

void on_write_porxy(uv_write_t *req, int status) {
    if (status < 0) {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
    }

    free(req->data);
    free(req);
}

void read_data_porxy(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if (nread <= 0) {
        if (nread != UV_EOF) fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t *)stream, close_cb_porxy);
        return;
    }

    int index = -1;
    for (int i = 0; i < stack; i++) {
        if (clients[i].targetClient.handle == stream) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        fprintf(stderr, "read_data_porxy, 소켓을 찾을 수 없음\n");
        return;
    }

    printf("read_data_porxy, host: %s, addr: %p\n", clients[index].host, &clients[index]);

    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    write_req->data = buf->base;
    uv_write(write_req, clients[index].Client, buf, 1, on_write_porxy);
}

void on_connect_porxy(uv_connect_t *req, int status) {
    if (status < 0) {
        fprintf(stderr, "on_connect_porxy, 연결오류 %s\n", uv_strerror(status));
        return;
    }

    ClientList *target = (ClientList *)req;
    printf("on_connect_porxy, host: %s, addr: %p\n", target->host, &target->targetClient);

    uv_read_start(target->targetClient.handle, alloc_buffer_porxy, read_data_porxy);

    // 성공적으로 대상 서버에 연결을 하면 프록시 서버에 연결된 클라이언트에 Established 응답을 보냄
    // strdup을 쓰는 이유는 on_write_porxy에서 메모리를 free 시키는데 established는 스택 영역에 선언된 상수라서 free 시키면 오류가 발생함
    uv_buf_t resBuffer = uv_buf_init(strdup(established), strlen(established));
    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    write_req->data = resBuffer.base;
    uv_write(write_req, target->Client, &resBuffer, 1, on_write_porxy);
}

void sendTargetServer(uv_stream_t *clientStream, const uv_buf_t *buf, ssize_t nread) {
    int index = -1;
    for (int i = 0; i < stack; i++) {
        if (clients[i].Client == clientStream) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        fprintf(stderr, "sendTargetServer, 소켓을 찾을 수 없음\n");
        return;
    }

    printf("sendTargetServer, host: %s, addr: %p\n", clients[index].host, &clients[index].targetClient);
    // 프록시 서버에 연결된 클라이언트의 요청을 대상 서버에 전달
    uv_write_t *write_req = (uv_write_t *)malloc(sizeof(uv_write_t));
    write_req->data = buf->base;
    uv_write(write_req, clients[index].targetClient.handle, buf, 1, on_write_porxy);
}

void ConnectTargetServer(char *addr, int port, uv_stream_t *clientStream) {
    struct sockaddr_in dest;
    ClientList *client = (ClientList *)malloc(sizeof(ClientList));
    client->Client = clientStream;
    strcpy(client->host, addr);

    uv_tcp_init(loop, &client->handle);
    uv_ip4_addr(addr, port, &dest);
    uv_tcp_connect(&client->targetClient, &client->handle, (const struct sockaddr *)&dest, on_connect_porxy);
    add_client(client);
}
