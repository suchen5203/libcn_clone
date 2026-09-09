/* config.c - libdlc.data 解析 */
#define _GNU_SOURCE
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static dlc_config_t g_cfg;
static int          g_ready = 0;

static void log_err(const char* tag, const char* fmt, ...) {
    fprintf(stderr, "[libcn_clone:config] %s: ", tag);
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

int dlc_config_load(const char* path, dlc_config_t* cfg) {
    if (!cfg) return -1;
    memset(cfg, 0, sizeof(*cfg));

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        log_err("open", "open(%s) failed: %s", path, strerror(errno));
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        log_err("fstat", "%s", strerror(errno));
        close(fd); return -1;
    }
    if ((size_t)st.st_size != DLC_TOTAL_SIZE) {
        log_err("size", "expected %u bytes, got %ld",
                DLC_TOTAL_SIZE, (long)st.st_size);
        close(fd); return -1;
    }

    void* map = mmap(NULL, DLC_TOTAL_SIZE,
                     PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        log_err("mmap", "%s", strerror(errno));
        close(fd); return -1;
    }
    close(fd);

    /* Header */
    memcpy(&cfg->header, map, DLC_HEADER_SIZE);
    if (cfg->header.version != DLC_MAGIC_VERSION) {
        log_err("ver", "expected %u, got %u (libdlc.data ver is err)",
                DLC_MAGIC_VERSION, cfg->header.version);
        munmap(map, DLC_TOTAL_SIZE); return -1;
    }
    if (cfg->header.count_a != DLC_TABLE_A_COUNT) {
        log_err("count_a", "expected %u, got %u",
                DLC_TABLE_A_COUNT, cfg->header.count_a);
        munmap(map, DLC_TOTAL_SIZE); return -1;
    }

    /* Table A */
    const uint8_t* p = (const uint8_t*)map + DLC_HEADER_SIZE;
    for (uint32_t i = 0; i < DLC_TABLE_A_COUNT; i++) {
        memcpy(&cfg->table_a[i], p, DLC_TABLE_A_ENTRY_SZ);
        p += DLC_TABLE_A_ENTRY_SZ;
    }

    /* Table B */
    for (uint32_t i = 0; i < DLC_TABLE_B_COUNT; i++) {
        memcpy(&cfg->table_b[i], p, DLC_TABLE_B_ENTRY_SZ);
        p += DLC_TABLE_B_ENTRY_SZ;
    }

    /* Main Body - 整段引用,不复制 */
    cfg->main_body      = (uint8_t*)map;
    cfg->main_body_size = DLC_TOTAL_SIZE;

    /* 统计 Main Body 非零条目(单次扫描,后续可缓存) */
    uint32_t cnt = 0;
    for (size_t off = DLC_HEADER_SIZE
                      + DLC_TABLE_A_COUNT * DLC_TABLE_A_ENTRY_SZ
                      + DLC_TABLE_B_COUNT * DLC_TABLE_B_ENTRY_SZ;
         off + DLC_MAIN_ENTRY_SZ <= DLC_TOTAL_SIZE;
         off += DLC_MAIN_ENTRY_SZ)
    {
        const dlc_main_entry_t* e = (const dlc_main_entry_t*)(map + off);
        if (e->id != 0 || e->value != 0.0f) cnt++;
    }
    cfg->main_count = cnt;

    /* 拷贝到全局(结构体整体复制,再单独修正指针/大小/计数) */
    memcpy(&g_cfg, cfg, sizeof(*cfg));
    g_cfg.main_body      = (uint8_t*)map;
    g_cfg.main_body_size = cfg->main_body_size;
    g_cfg.main_count     = cnt;
    g_ready = 1;

    log_err("ok", "loaded ver=%u count_a=%u main=%u",
            cfg->header.version, cfg->header.count_a, cnt);
    return 0;
}

void dlc_config_free(dlc_config_t* cfg) {
    if (!cfg) return;
    if (cfg->main_body && cfg->main_body_size > 0) {
        munmap(cfg->main_body, cfg->main_body_size);
    }
    memset(cfg, 0, sizeof(*cfg));
    g_ready = 0;
}

float dlc_lookup_table_a(uint32_t id) {
    if (!g_ready) return 1.0f;
    if (id == 0 || id > DLC_TABLE_A_COUNT) return 1.0f;
    return g_cfg.table_a[id - 1].f1;
}

float dlc_lookup_table_b_neg(uint32_t id) {
    if (!g_ready) return 0.0f;
    if (id == 0 || id > DLC_TABLE_B_COUNT) return 0.0f;
    return g_cfg.table_b[id - 1].neg;
}

float dlc_lookup_table_b_pos(uint32_t id) {
    if (!g_ready) return 0.0f;
    if (id == 0 || id > DLC_TABLE_B_COUNT) return 0.0f;
    return g_cfg.table_b[id - 1].pos;
}

float dlc_lookup_main(uint32_t id) {
    if (!g_ready) return 0.0f;
    if (!g_cfg.main_body) return 0.0f;

    /* 线性扫描 - 启动期可改为 hash */
    for (size_t off = DLC_HEADER_SIZE
                      + DLC_TABLE_A_COUNT * DLC_TABLE_A_ENTRY_SZ
                      + DLC_TABLE_B_COUNT * DLC_TABLE_B_ENTRY_SZ;
         off + DLC_MAIN_ENTRY_SZ <= g_cfg.main_body_size;
         off += DLC_MAIN_ENTRY_SZ)
    {
        const dlc_main_entry_t* e =
            (const dlc_main_entry_t*)(g_cfg.main_body + off);
        if (e->id == id) return e->value;
        if (e->id > id) break;  /* 假设 id 单调,可二分优化 */
    }
    return 0.0f;
}

int dlc_is_ready(void) { return g_ready; }

const dlc_config_t* dlc_get_config(void) { return &g_cfg; }