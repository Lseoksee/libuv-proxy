#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HEADERS 100
#define MAX_LINE_LENGTH 1024

typedef struct {
    char *key;
    char *value;
} Header;

typedef struct {
    char method[16];
    char url[256];
    char version[16];
    Header headers[MAX_HEADERS];
    int header_count;
} HttpRequest;

typedef struct {
    char *url;
    char *port;
} URL;

void free_headers(HttpRequest *req);
void parse_http_request(const char *raw_request, int len, HttpRequest *req);
void parseURL(char *hostURL, URL *res);

/** ip 주소인지 판단 (v4, v6 둘다 가능) */
int is_ip(const char *input);