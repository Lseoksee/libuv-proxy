#include "Global.h"
#include "ServerLog.h"
#include "Embedded_help.txt.h"

extern ServerConfig SERVER_CONFIG;

void __save_log_local(const char *__restrict__ _Format, ...);

void print_help() { printf("%s", HELP_TXT); }

void put_log(LOG_TYPE log_type, const char *__restrict__ _Format, ...) {
    if (SERVER_CONFIG.noLog && log_type != LOG_INFO_FORCE && log_type != LOG_WARNING_FORCE &&
        log_type != LOG_ERROR_FORCE) {
        return;
    }

    // 가변 인자 파싱
    va_list args;
    va_start(args, _Format);
    int len = vsnprintf(NULL, 0, _Format, args);
    va_end(args);

    char buf[len + 1];

    va_start(args, _Format);
    vsnprintf(buf, len + 1, _Format, args);
    va_end(args);

    switch (log_type) {
        case LOG_INFO:
        case LOG_INFO_FORCE:
            printf("%s%s%s\n", COLOR_RESET, buf, COLOR_RESET);
            break;
        case LOG_WARNING:
        case LOG_WARNING_FORCE:
            printf("%s%s%s\n", COLOR_YELLOW, buf, COLOR_RESET);
            break;
        case LOG_ERROR:
        case LOG_ERROR_FORCE:
            printf("%s%s%s\n", COLOR_RED, buf, COLOR_RESET);
            break;
        default:
            break;
    }

    __save_log_local("%s\n", buf);
}

void put_time_log(LOG_TYPE log_type, const char *__restrict__ _Format, ...) {
    if (SERVER_CONFIG.noLog && log_type != LOG_INFO_FORCE && log_type != LOG_WARNING_FORCE &&
        log_type != LOG_ERROR_FORCE) {
        return;
    }

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

    va_start(args, _Format);
    vsnprintf(buf, len + 1, _Format, args);
    va_end(args);

    switch (log_type) {
        case LOG_INFO:
        case LOG_INFO_FORCE:
            printf("%s%s %s%s\n", COLOR_RESET, time_str, buf, COLOR_RESET);
            break;
        case LOG_WARNING:
        case LOG_WARNING_FORCE:
            printf("%s%s %s%s\n", COLOR_YELLOW, time_str, buf, COLOR_RESET);
            break;
        case LOG_ERROR:
        case LOG_ERROR_FORCE:
            printf("%s%s %s%s\n", COLOR_RED, time_str, buf, COLOR_RESET);
            break;
        default:
            break;
    }

    __save_log_local("%s %s: %s\n", time_str, buf);
}

void put_ip_log(LOG_TYPE log_type, const char *ip_addr, const char *__restrict__ _Format, ...) {
    if (SERVER_CONFIG.noLog && log_type != LOG_INFO_FORCE && log_type != LOG_WARNING_FORCE &&
        log_type != LOG_ERROR_FORCE) {
        return;
    }

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

    va_start(args, _Format);
    vsnprintf(buf, len + 1, _Format, args);
    va_end(args);

    switch (log_type) {
        case LOG_INFO:
        case LOG_INFO_FORCE:
            printf("%s%s %s: %s%s\n", COLOR_RESET, time_str, ip_addr, buf, COLOR_RESET);
            break;
        case LOG_WARNING:
        case LOG_WARNING_FORCE:
            printf("%s%s %s: %s%s\n", COLOR_YELLOW, time_str, ip_addr, buf, COLOR_RESET);
            break;
        case LOG_ERROR:
        case LOG_ERROR_FORCE:
            printf("%s%s %s: %s%s\n", COLOR_RED, time_str, ip_addr, buf, COLOR_RESET);
            break;
        default:
            break;
    }

    __save_log_local("%s %s: %s\n", time_str, ip_addr, buf);
}

void __save_log_local(const char *__restrict__ _Format, ...) {
    // 가변 인자 파싱
    va_list args;
    va_start(args, _Format);
    int len = vsnprintf(NULL, 0, _Format, args);
    va_end(args);

    char buf[len + 1];

    va_start(args, _Format);
    vsnprintf(buf, len + 1, _Format, args);
    va_end(args);

    FILE *fp = fopen("server.log", "a+");
    if (fp == NULL) {
        return;
    }

    fputs(buf, fp);
    fclose(fp);
}