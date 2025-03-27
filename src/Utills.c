#include "Utills.h"

void print_help() {
    FILE *file = fopen(DATA_FILE, "r");
    if (file == NULL) {
        return;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file)) {
        printf("%s", buffer);
    }

    fclose(file);
    return;
}

int is_ip(const char *input) {
    struct in_addr ipv4;
    struct in6_addr ipv6;
    return inet_pton(AF_INET, input, &ipv4) || inet_pton(AF_INET6, input, &ipv6);
}
