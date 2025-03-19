#include "ParseSNI.h"

int init = 0;
int is_clientHello(const char *buf, ssize_t nread) {
    if (nread > 5 && (unsigned char)buf[0] == 0x16) {  // TLS Handshake 패킷
        int tls_version = (buf[1] << 8) | (buf[2]);

        if (tls_version == 0x0301 || tls_version == 0x0302 || tls_version == 0x0303 || tls_version == 0x0304) {
            if (buf[5] == 0x01) {
                return 1;
            }
        }
    }

    return 0;
}

resultSNI encrypt_sni_from_client_hello(const char *buf, ssize_t nread) {
    resultSNI result;
    result.result_buf = (char *)malloc(nread);

    // 전체 패킷을 먼저 결과 버퍼에 복사
    memcpy(result.result_buf, buf, nread);
    // Handshake 메시지 길이
    uint32_t handshake_length = (buf[6] << 16) | (buf[7] << 8) | buf[8];

    // TLS 버전 스킵
    const uint8_t *p = (const uint8_t *)&buf[11];        // 5(TLS 레코드 헤더) + 4(Handshake 헤더) + 2(Client Version)
    uint8_t *p_res = (uint8_t *)&result.result_buf[11];  // 결과 버퍼의 동일한 위치

    // Random 스킵 (32 bytes)
    p += 32;
    p_res += 32;

    // Session ID 길이
    uint8_t session_id_length = *p;
    p += 1 + session_id_length;
    p_res += 1 + session_id_length;

    // Cipher Suites 길이
    uint16_t cipher_suites_length = (p[0] << 8) | p[1];
    p += 2 + cipher_suites_length;
    p_res += 2 + cipher_suites_length;

    // Compression Methods 길이
    uint8_t compression_methods_length = *p;
    p += 1 + compression_methods_length;
    p_res += 1 + compression_methods_length;

    // 확장 필드 길이
    uint16_t extensions_length = (p[0] << 8) | p[1];
    p += 2;
    p_res += 2;

    // 확장 필드 순회
    const uint8_t *extensions_end = p + extensions_length;
    while (p < extensions_end) {
        // 확장 타입
        uint16_t extension_type = (p[0] << 8) | p[1];
        p += 2;
        p_res += 2;

        // 확장 길이
        uint16_t extension_length = (p[0] << 8) | p[1];
        p += 2;
        p_res += 2;

        // SNI 확장 타입 = 0x0000
        if (extension_type == 0x0000) {
            // SNI 리스트 길이 위치 저장
            uint8_t *sni_list_length_pos = p_res - 2;
            uint16_t original_extension_length = extension_length;

            // SNI 리스트 길이
            uint16_t sni_list_length = (p[0] << 8) | p[1];
            p += 2;
            p_res += 2;

            if (sni_list_length > 0) {
                // SNI 타입 (hostname = 0)
                uint8_t name_type = p[0];
                p += 1;
                p_res += 1;

                if (name_type == 0) {  // hostname
                    // hostname 길이
                    uint16_t name_length = (p[0] << 8) | p[1];
                    uint8_t *hostname_length_pos = p_res;  // 호스트명 길이 위치 저장
                    p += 2;
                    p_res += 2;

                    // hostname 추출 및 해싱

                    unsigned char hostname[256] = {0};         // 호스트명 저장 버퍼
                    if (name_length > 255) name_length = 255;  // 버퍼 오버플로우 방지

                    // 호스트명 복사
                    memcpy(hostname, p, name_length);
                    hostname[name_length] = '\0';  // 문자열 종료

                    // SHA-256 해싱
                    unsigned char hash[64];
                    SHA256_CTX ctx;
                    sha256_init(&ctx);
                    sha256_update(&ctx, hostname, name_length);
                    sha256_final(&ctx, hash);

                    // 해시 결과를 16진수 문자열로 변환
                    char hash_hex[SHA256_BLOCK_SIZE * 2 + 1] = {0};
                    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
                        sprintf(&hash_hex[i * 2], "%02x", hash[i]);
                    }

                    uint16_t sha_len = SHA256_BLOCK_SIZE * 2;
                    for (int i = 0; i < name_length; i++) {
                        p_res[i] = hash_hex[i % sha_len];
                    }

                    memcpy(result.afterSNI, p_res, name_length);
                    memcpy(result.beforeSNI, hostname, name_length);
                }
            }

            break;  // SNI 확장을 찾았음
        }

        // 다음 확장으로 이동
        p += extension_length;
        p_res += extension_length;
    }

    return result;
}

void free_resultSNI(resultSNI *result) {
    free(result->result_buf);
    result->result_buf = NULL;
}