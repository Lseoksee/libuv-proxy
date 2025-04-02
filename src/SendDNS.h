#pragma once

struct dns_request_t;
struct dns_response_t;

typedef void (*dns_query_cb)(struct dns_response_t* dns);

typedef struct dns_response_s {
    /** 응답 상태
     * `1`: 성공,
     * `-1`: timeout,
     * `-2`: 해당 호스트의 IP를 찾을 수 없음
     */
    int status;
    uint64_t timeout;
    char* dns_address;
    char* port;
    char* hostname;
    char ip_address[INET6_ADDRSTRLEN];
} dns_response_s;

typedef struct dns_response_t {
    dns_response_s dns_response;
    Client* client_data;
} dns_response_t;

typedef struct uv_udp_t_a {
    /* `0`: 활성화 중, `-1` 종료됨 */
    int state;
    uv_udp_t handle;
} uv_udp_t_a;

typedef struct uv_timer_t_a {
    /* `0`: 활성화 중, `-1` 종료됨 */
    int state;
    uv_timer_t timer;
} uv_timer_t_a;

// DNS 구조체
typedef struct dns_request_t {
    uv_udp_t_a udp_handle;
    uv_timer_t_a timeout_timer;
    int query_type;
    dns_query_cb cb;
    dns_response_t res;
} dns_request_t;

/** DNS 서버에 요청합니다
 * @return
 * `1`: 성공,
 * `-1`: DNS 주소 오류,
 * `-2`: 소켓 바인딩 실패,
 * `-3`: UDP 수신 시작 실패,
 * `-4`: UDP 전송 실패
 */
int send_dns_query(uv_loop_t* loop, char* hostname, char* port, char* dns_server, int query_type, uint64_t timeout, Client* client, dns_query_cb cb);

/** 기본 DNS 서버로 요청합니다\n
 * `Client` 구조체의 `host`, `port` 필드 값이 초기화 되어야 합니다
 * @return
 * `1`: 성공,
 * `-1`: DNS 요청 실패,
 */
int send_default_dns(uv_loop_t* loop, char* hostname, const char* port, Client* client, dns_query_cb cb);