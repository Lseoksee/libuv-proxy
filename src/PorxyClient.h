#pragma once

/** PorxyClient 초기화 */
void init_PorxyClient(uv_loop_t *mainLoop);

/** 타겟 서버에 연결 시도 */
int ConnectTargetServer(char *addr, int port, Client *client);

/** 타겟 서버에 데이터 전송 */
void sendTargetServer(uv_stream_t *clientStream, const char*buf, ssize_t nread);