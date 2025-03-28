#include "ServerLog.h"

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

void put_log(LOG_TYPE log_type, const char *__restrict__ _Format, ...) {
    // 가변 인자 파싱
    va_list args;
    va_start(args, _Format);
    int len = vsnprintf(NULL, 0, _Format, args);
    va_end(args);

    char buf[len + 1];
    vsnprintf(buf, len + 1, _Format, args);

    switch (log_type) {
        case LOG_INFO:
            printf("%s%s%s\n", COLOR_RESET, buf, COLOR_RESET);
            break;
        case LOG_WARNING:
            printf("%s%s%s\n", COLOR_YELLOW, buf, COLOR_RESET);
            break;
        case LOG_ERROR:
            printf("%s%s%s\n", COLOR_RED, buf, COLOR_RESET);
            break;
        default:
            break;
    }
}

void put_time_log(LOG_TYPE log_type, const char *__restrict__ _Format, ...) {
    // 현재 시간 가져오기
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S]", local_time);

    // 가변 인자 파싱
    va_list args;
    va_start(args, _Format);
    int len = vsnprintf(NULL, 0, _Format, args);
    va_end(args);

    char buf[len + 1];
    vsnprintf(buf, len + 1, _Format, args);

    switch (log_type) {
        case LOG_INFO:
            printf("%s%s %s%s\n", COLOR_RESET, time_str, buf, COLOR_RESET);
            break;
        case LOG_WARNING:
            printf("%s%s %s%s\n", COLOR_YELLOW, time_str, buf, COLOR_RESET);
            break;
        case LOG_ERROR:
            printf("%s%s %s%s\n", COLOR_RED, time_str, buf, COLOR_RESET);
            break;
        default:
            break;
    }
}

void put_ip_log(LOG_TYPE log_type, const char* ip_addr, const char *__restrict__ _Format, ...) {
    // 현재 시간 가져오기
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S]", local_time);

    // 가변 인자 파싱
    va_list args;
    va_start(args, _Format);
    int len = vsnprintf(NULL, 0, _Format, args);
    va_end(args);

    char buf[len + 1];
    vsnprintf(buf, len + 1, _Format, args);

    switch (log_type) {
        case LOG_INFO:
            printf("%s%s %s: %s%s\n", COLOR_RESET, time_str, ip_addr, buf, COLOR_RESET);
            break;
        case LOG_WARNING:
            printf("%s%s %s: %s%s\n", COLOR_YELLOW, time_str, ip_addr,  buf, COLOR_RESET);
            break;
        case LOG_ERROR:
            printf("%s%s %s: %s%s\n", COLOR_RED, time_str, ip_addr, buf, COLOR_RESET);
            break;
        default:
            break;
    }
}