#pragma once

#ifndef DATA_FILE
#define DATA_FILE "help.txt"
#endif

#include "Global.h"

void print_help();

/** ip 인지 여부 확인 */
int is_ip(const char *input);
