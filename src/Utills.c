#include "Global.h"
#include "Utills.h"

void get_client_ip(uv_stream_t* client, char* ip_str, size_t ip_str_len) {
    struct sockaddr_storage addr;
    int addr_len = sizeof(addr);

    // 클라이언트 소켓의 피어 이름(IP 주소) 가져오기
    uv_tcp_getpeername((uv_tcp_t*)client, (struct sockaddr*)&addr, &addr_len);

    // IPv4와 IPv6 처리
    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* sock_v4 = (struct sockaddr_in*)&addr;
        inet_ntop(AF_INET, &(sock_v4->sin_addr), ip_str, ip_str_len);
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6* sock_v6 = (struct sockaddr_in6*)&addr;
        inet_ntop(AF_INET6, &(sock_v6->sin6_addr), ip_str, ip_str_len);
    }
}

int is_ip(const char* input) {
    struct in_addr ipv4;
    struct in6_addr ipv6;
    return inet_pton(AF_INET, input, &ipv4) || inet_pton(AF_INET6, input, &ipv6);
}

void removeSpace(char* str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src != ' ') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}
