#pragma once

#include "Global.h"

/** PorxyClient 초기화 */
void init_PorxyClient(uv_loop_t *mainLoop);

/** 타겟 서버에 데이터 읽기 */
void read_data_porxy(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);

/** 타겟 서버 연결 완료 */
void on_connect_porxy(uv_connect_t *req, int status);

/** 타겟 서버에 데이터 전송 */
void sendTargetServer(uv_stream_t *clientStream, const uv_buf_t *buf, ssize_t nread);

/** 타겟 서버에 연결 시도 */
void ConnectTargetServer(char *addr, int port, uv_stream_t *clientStream);