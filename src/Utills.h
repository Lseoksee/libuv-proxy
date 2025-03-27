#pragma once

#ifndef DATA_FILE
#define DATA_FILE "help.txt"
#endif

typedef enum {
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3
} LOG_TYPE;

#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_YELLOW  "\x1b[33m"

#include "Global.h"

void print_help();

void put_log(LOG_TYPE log_type, const char *__restrict__ _Format, ...);

void put_time_log(LOG_TYPE log_type, const char *__restrict__ _Format, ...);

/** ip 인지 여부 확인 */
int is_ip(const char *input);
