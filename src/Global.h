#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <uv.h>

/** 연결 요청 대기 큐 최대길이 (리눅스 기본값 128개) */
#define DEFAULT_BACKLOG 128
#define DEFAULT_PORT 1603

/** 버퍼 할당 함수 */
void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);

/** 연결 종료 콜백 */
void close_cb(uv_handle_t *handle);

/** 데이터 전송 콜백 */
void on_write(uv_write_t *req, int status);
