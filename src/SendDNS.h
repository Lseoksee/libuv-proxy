#pragma once

#include "Global.h"

struct dns_request_t;
struct dns_response_t;

typedef void (*dns_query_cb)(struct dns_response_t* dns);

typedef struct dns_response_t {
    /** 응답 상태
     * `1`: 성공,
     * `-1`: timeout,
     * `-2`: 해당 호스트의 IP를 찾을 수 없음
     */
    int status;
    int port;
    char* hostname;
    char ip_address[INET6_ADDRSTRLEN];
    uv_stream_t* clientStream;
    struct dns_request_t* req;
} dns_response_t;

// DNS 구조체
typedef struct dns_request_t {
    uv_udp_t udp_handle;
    uv_timer_t timeout_timer;
    int query_type;
    dns_response_t res;
    dns_query_cb cb;
} dns_request_t;

/** DNS 서버에 요청합니다
 * @return
 * `1`: 성공,
 * `-1`: DNS 주소 오류,
 * `-2`: 소켓 바인딩 실패,
 * `-3`: UDP 수신 시작 실패,
 * `-4`: UDP 전송 실패
 */
int send_dns_query(uv_loop_t* loop, const char* hostname, const char* dns_server, int query_type, dns_response_t res, dns_query_cb cb);

void free_dns(dns_response_t* res);

/** 기본 DNS 서버로 요청합니다
 * @return
 * `1`: 성공,
 * `-1`: DNS 요청 실패,
 */
int send_default_dns(uv_loop_t* loop, const char* host, const char* port, dns_response_t* dns_res);