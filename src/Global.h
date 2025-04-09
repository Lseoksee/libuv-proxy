#pragma once

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <uv.h>
/* 리눅스 대응 */
#ifdef __unix
#include <stdarg.h>
#endif

typedef enum {
    PROXY_HTTPS,
    PROXY_HTTP,
} ProxyMode;

typedef struct {
    /** 클라이언트 상태
     * `0`: 최초 연결
     * `1`: 프록시 연결 완료 후 데이터 교환
     */
    int state;
    /** 프록시 서버가 대상 서버에 연결하기 위한 클라이언트 (즉 요청을 위한)
     * uv_stream_t == uv_tcp_t  주소값은 같지만 uv_tcp_t가 자식
     */
    uv_tcp_t targetClient;
    /** 프록시 서버에 접속하는 클라이언트 (즉 응답을 위한)
     * uv_stream_t == uv_tcp_t 주소값은 같지만 uv_tcp_t가 자식
     */
    uv_tcp_t proxyClient;
    char *host;
    char ClientIP[INET6_ADDRSTRLEN];
    ProxyMode connect_mode;
    /** HTTP 전송용 버퍼  */
    uv_buf_t send_buf;
    /** 메모리 할당 헤제용 */
    uv_connect_t target_connecter;
    /** 타임아웃 타이머 */
    uv_timer_t timeout_timer;
    /** 레퍼런스 카운트 */
    int ref_count;
} Client;

typedef struct {
    char *dns_1;
    char *dns_2;
} DnsOptions;

/** 연결 요청 대기 큐 최대길이 (리눅스 기본값 128개) */
#define DEFAULT_BACKLOG 128

/** 버퍼 할당 함수 */
void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);

/** 연결 종료 콜백 */
void close_cb(uv_handle_t *handle);

/** 데이터 전송 콜백 */
void on_write(uv_write_t *req, int status);

/** 종료 플래그 전송 */
void on_shutdown(uv_shutdown_t *req, int status);

void ref_client(Client *client);
void unref_client(Client *client);