#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SNI_MAX_SIZE 256

typedef struct resultSNI {
    char *result_buf;
    char beforeSNI[SNI_MAX_SIZE];
    char afterSNI[SNI_MAX_SIZE];
} resultSNI;

/** 해당 패킷이 clientHello 인지 판단합니다  */
int is_clientHello(const char *buf, ssize_t nread);

/** SNI 필드를 SHA-256으로 해싱합니다. */
resultSNI encrypt_sni_from_client_hello(const char *buf, ssize_t nread);

/** resultSNI를 정리합니다 */
void free_resultSNI(resultSNI *result);