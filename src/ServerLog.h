#pragma once

#ifndef DATA_FILE
#define DATA_FILE "help.txt"
#endif

#define COLOR_RESET "\x1b[0m"
#define COLOR_RED "\x1b[31m"
#define COLOR_YELLOW "\x1b[33m"

typedef enum {
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3,
    LOG_INFO_FORCE = 4,
    LOG_WARNING_FORCE = 5,
    LOG_ERROR_FORCE = 6,
} LOG_TYPE;

void print_help();
void put_log(LOG_TYPE log_type, const char *__restrict__ _Format, ...);
void put_time_log(LOG_TYPE log_type, const char *__restrict__ _Format, ...);
void put_ip_log(LOG_TYPE log_type, const char *ip_addr, const char *__restrict__ _Format, ...);