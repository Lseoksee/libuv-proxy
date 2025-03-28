#pragma once

#include "Global.h"

/** 클라이언트 ip 주소 구하기 */
void get_client_ip(uv_stream_t* client, char* ip_str, size_t ip_str_len);

/** ip 인지 여부 확인 */
int is_ip(const char *input);
