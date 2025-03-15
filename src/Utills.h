#pragma once

#include "Global.h"

/** 도메인 -> IPv4 */
char *getDnsToAddr(uv_loop_t *loop, const char *host, const char *port);

/** ip 인지 여부 확인 */
int is_ip(const char *input);
