#include <uv.h>

char *getDnsToAddr(uv_loop_t *loop, const char *host, const char *port) {
    uv_getaddrinfo_t *res;

    struct addrinfo hints;
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    int status = uv_getaddrinfo(loop, res, NULL, host, port, &hints);
    if (status != 0) {
        return NULL;
    }

    void *addr_ptr;
    char addr[INET_ADDRSTRLEN];

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->addrinfo->ai_addr;
    addr_ptr = &(ipv4->sin_addr);
    inet_ntop(res->addrinfo->ai_family, addr_ptr, addr, sizeof(addr));

    char *resAddr = (char *)malloc(strlen(addr) + 1);
    strcpy_s(resAddr, INET_ADDRSTRLEN, addr);

    uv_freeaddrinfo(res->addrinfo);

    return resAddr;
}