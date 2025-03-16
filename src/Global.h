#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <uv.h>

typedef struct {
    /** 프록시 서버가 대상 서버에 연결하기 위한 클라이언트 (즉 요청을 위한) */
    uv_stream_t *targetClient;
    /** 프록시 서버에 접속하는 클라이언트 (즉 응답을 위한) */
    uv_stream_t *proxyClient;
    char host[1024];
} Client;

/** 연결 요청 대기 큐 최대길이 (리눅스 기본값 128개) */
#define DEFAULT_BACKLOG 128
#define DEFAULT_PORT 1603

/** 버퍼 할당 함수 */
void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);

/** 연결 종료 콜백 */
void close_cb(uv_handle_t *handle);

/** 데이터 전송 콜백 */
void on_write(uv_write_t *req, int status);

/** 종료 플래그 전송 */
void on_shutdown(uv_shutdown_t* req, int status);