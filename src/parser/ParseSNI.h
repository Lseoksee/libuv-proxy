#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sha256.h"

typedef struct resultSNI {
    char *result_buf;
    char beforeSNI[1024];
    char afterSNI[1024];
} resultSNI;

/** 해당 패킷이 clientHello 인지 판단합니다  */
int is_clientHello(const char *buf, ssize_t nread);

/** SNI 필드를 SHA-256으로 해싱합니다. */
resultSNI encrypt_sni_from_client_hello(const char *buf, ssize_t nread);

/** resultSNI를 정리합니다 */
void free_resultSNI(resultSNI *result);