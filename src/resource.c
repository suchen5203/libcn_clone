/* resource.c - resource.data 解密 + 切片
 * 默认实现:AES-128-CBC(需 -lcrypto)
 * 也可改为 XOR / 自定义算法,改 decrypt() 即可。
 */
#define _GNU_SOURCE
#include "resource.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if OpenSSL
#include <openssl/aes.h>
#include <openssl/evp.h>
#endif

#define RESOURCE_MAGIC       "DLCr"
#define RESOURCE_MAGIC_SZ    4
#define RESOURCE_IV_SZ       16
#define RESOURCE_KEY_SZ      16
#define RESOURCE_FILE_SZ     65535u     /* 0xFFFF */
#define MAX_SECTIONS         8

static resource_section_t g_sections[MAX_SECTIONS];
static int                g_section_count = 0;
static int                g_ready = 0;
static uint8_t            g_key[RESOURCE_KEY_SZ];
static int                g_key_loaded = 0;

static void log_err(const char* tag, const char* fmt, ...) {
    fprintf(stderr, "[libcn_clone:resource] %s: ", tag);
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

/* ===  加载密钥  === */
/* 默认从 ./libcn.key 读 16 字节;
 * 也可改成硬编码 / 环境变量 / 远程拉取 */
static int load_key(void) {
    if (g_key_loaded) return 0;
    const char* env = getenv("LIBCN_KEY_PATH");
    const char* path = env ? env : "./libcn.key";

    FILE* f = fopen(path, "rb");
    if (!f) {
        log_err("key", "open %s failed, using fallback", path);
        /* 回退:用全 0 密钥(仅 dev 用,prod 必接真密钥) */
        memset(g_key, 0, RESOURCE_KEY_SZ);
        g_key_loaded = 1;
        return 0;
    }
    size_t n = fread(g_key, 1, RESOURCE_KEY_SZ, f);
    fclose(f);
    if (n != RESOURCE_KEY_SZ) {
        log_err("key", "short read %zu", n);
        return -1;
    }
    g_key_loaded = 1;
    return 0;
}

/* ===  AES-128-CBC 解密(OpenSSL EVP 接口)  === */
#if OpenSSL
static int aes128_cbc_decrypt(const uint8_t* in, size_t in_len,
                              const uint8_t* key, const uint8_t* iv,
                              uint8_t* out, size_t* out_len)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    int len = 0, total = 0;
    int ok = 1;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv) != 1) ok = 0;
    if (ok && EVP_CIPHER_CTX_set_padding(ctx, 1) != 1) ok = 0;

    if (ok && EVP_DecryptUpdate(ctx, out, &len, in, (int)in_len) != 1) ok = 0;
    total = len;

    if (ok && EVP_DecryptFinal_ex(ctx, out + len, &len) != 1) ok = 0;
    total += len;

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) return -1;
    *out_len = (size_t)total;
    return 0;
}
#else
/* 无 OpenSSL 时的占位实现:XOR(纯 dev 用) */
static int aes128_cbc_decrypt(const uint8_t* in, size_t in_len,
                              const uint8_t* key, const uint8_t* iv,
                              uint8_t* out, size_t* out_len)
{
    (void)iv;
    for (size_t i = 0; i < in_len; i++) {
        out[i] = in[i] ^ key[i % RESOURCE_KEY_SZ];
    }
    *out_len = in_len;
    return 0;
}
#endif

/* ===  解析明文  === */
static int parse_plain(const uint8_t* buf, size_t sz) {
    if (sz < RESOURCE_MAGIC_SZ + 4) return -1;
    if (memcmp(buf, RESOURCE_MAGIC, RESOURCE_MAGIC_SZ) != 0) {
        log_err("parse", "bad magic %.4s", (char*)buf);
        return -1;
    }
    uint32_t section_cnt = 0;
    memcpy(&section_cnt, buf + RESOURCE_MAGIC_SZ, 4);
    if (section_cnt == 0 || section_cnt > MAX_SECTIONS) {
        log_err("parse", "bad section count %u", section_cnt);
        return -1;
    }
    size_t off = RESOURCE_MAGIC_SZ + 4;
    g_section_count = 0;

    for (uint32_t i = 0; i < section_cnt && g_section_count < MAX_SECTIONS; i++) {
        if (off + 4 > sz) return -1;
        uint32_t name_len = 0;
        memcpy(&name_len, buf + off, 4);
        off += 4;
        if (name_len == 0 || name_len >= 64 || off + name_len > sz) return -1;
        memcpy(g_sections[g_section_count].name, buf + off, name_len);
        g_sections[g_section_count].name[name_len] = '\0';
        off += name_len;

        if (off + 4 > sz) return -1;
        uint32_t data_len = 0;
        memcpy(&data_len, buf + off, 4);
        off += 4;
        if (off + data_len > sz) return -1;

        g_sections[g_section_count].data = (uint8_t*)malloc(data_len);
        if (!g_sections[g_section_count].data) return -1;
        memcpy(g_sections[g_section_count].data, buf + off, data_len);
        g_sections[g_section_count].len = data_len;
        off += data_len;
        g_section_count++;
    }
    return 0;
}

/* ===  公开 API  === */
int resource_load(const char* path) {
    if (load_key() != 0) return -1;

    FILE* f = fopen(path, "rb");
    if (!f) { log_err("open", "%s: %s", path, strerror(errno)); return -1; }

    uint8_t cipher[RESOURCE_FILE_SZ];
    size_t n = fread(cipher, 1, RESOURCE_FILE_SZ, f);
    fclose(f);
    if (n != RESOURCE_FILE_SZ) {
        log_err("read", "short %zu", n);
        return -1;
    }

    uint8_t iv[RESOURCE_IV_SZ];
    memcpy(iv, cipher, RESOURCE_IV_SZ);

    uint8_t plain[RESOURCE_FILE_SZ];
    size_t  plain_len = 0;
    if (aes128_cbc_decrypt(cipher + RESOURCE_IV_SZ,
                           RESOURCE_FILE_SZ - RESOURCE_IV_SZ,
                           g_key, iv, plain, &plain_len) != 0) {
        log_err("decrypt", "failed");
        return -1;
    }

    if (parse_plain(plain, plain_len) != 0) {
        log_err("parse", "failed");
        return -1;
    }

    g_ready = 1;
    log_err("ok", "loaded %zu bytes, %d sections", plain_len, g_section_count);
    for (int i = 0; i < g_section_count; i++) {
        log_err("section", "%s (%u B)", g_sections[i].name, g_sections[i].len);
    }
    return 0;
}

void resource_unload(void) {
    for (int i = 0; i < g_section_count; i++) {
        if (g_sections[i].data) {
            memset(g_sections[i].data, 0, g_sections[i].len);
            free(g_sections[i].data);
        }
    }
    memset(g_sections, 0, sizeof(g_sections));
    g_section_count = 0;
    g_ready = 0;
}

const resource_section_t* resource_find(const char* name) {
    if (!g_ready || !name) return NULL;
    for (int i = 0; i < g_section_count; i++) {
        if (strcmp(g_sections[i].name, name) == 0) {
            return &g_sections[i];
        }
    }
    return NULL;
}

int resource_is_ready(void) { return g_ready; }