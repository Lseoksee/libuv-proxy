#include "Utills.h"

void getDnsToAddr(uv_loop_t *loop, const char *host, const char *port, char *res_buf) {
    uv_getaddrinfo_t res;

    struct addrinfo hints;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    int status = uv_getaddrinfo(loop, &res, NULL, host, port, &hints);
    if (status != 0) {
        return;
    }

    void *addr_ptr;
    char addr[INET_ADDRSTRLEN];

    struct sockaddr_in *ipv4 = (struct sockaddr_in *) res.addrinfo->ai_addr;
    addr_ptr = &(ipv4->sin_addr);
    inet_ntop(res.addrinfo->ai_family, addr_ptr, addr, sizeof(addr));

    strcpy_s(res_buf, INET_ADDRSTRLEN, addr);

    uv_freeaddrinfo(res.addrinfo);
}

int is_ip(const char *input) {
    struct in_addr ipv4;
    struct in6_addr ipv6;
    return inet_pton(AF_INET, input, &ipv4) || inet_pton(AF_INET6, input, &ipv6);
}
