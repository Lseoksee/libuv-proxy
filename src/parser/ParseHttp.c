#include "ParseHttp.h"

HttpRequest parse_http_request(const char *raw_request, int len) {
    HttpRequest req = {0};

    char buffer[len];
    strcpy(buffer, raw_request);  // 원본을 변경하지 않도록 복사

    char *line = strtok(buffer, "\r\n");  // 첫 번째 줄 파싱
    if (line == NULL) return req;

    // 요청 라인 파싱: "GET /index.html HTTP/1.1"
    sscanf(line, "%15s %255s %15s", req.method, req.url, req.version);

    // 헤더 파싱
    req.header_count = 0;
    while ((line = strtok(NULL, "\r\n")) && req.header_count < MAX_HEADERS) {
        char *colon = strchr(line, ':');
        if (!colon) continue;  // 콜론이 없으면 무시

        *colon = '\0';  // 키와 값을 분리
        char *key = line;
        char *value = colon + 1;

        while (*value == ' ') value++;  // 공백 제거

        req.headers[req.header_count].key = strdup(key);
        req.headers[req.header_count].value = strdup(value);
        req.header_count++;
    }
    req.state = 1;

    return req;
}

int is_connect_request(const char *request) { return strncmp(request, "CONNECT ", 8) == 0; }

URL parseURL(const char *hostURL) {
    URL res;
    char *temp = strdup(hostURL);

    // strchr는 특정 문자가 존재하는 위치를 반환한다
    //\0 은 C언어가 문장에 끝을 파악하는 기호인데 이를 이용하여 문자열을 나누는 것이다.
    // 그럼 hostURL은 : 이전, colon+1은 : 이후
    char *colon = strchr(temp, ':');
    
    //HTTP 인 경우 포트번호를 포함하지 않음
    if (colon == NULL) {
        res.url = temp;
        res.port = "80";
    } else {
        *colon = '\0';
        res.url = temp;
        res.port = colon + 1;
    }

    return res;
}

void freeHeader(HttpRequest *req) {
    for (int i = 0; i < req->header_count; i++) {
        free(req->headers[i].key);
        free(req->headers[i].value);
    }
}

void freeURL(URL *url) { free(url->url); }