/* config.h - libdlc.data 数据格式 + 查表接口 */
#ifndef DLC_CONFIG_H
#define DLC_CONFIG_H

#include <stdint.h>
#include <stddef.h>

/* ===  文件常量  === */
#define DLC_MAGIC_VERSION      57u
#define DLC_TABLE_A_COUNT      57u
#define DLC_TABLE_B_COUNT      21u     /* 估计值,需 calibrate */
#define DLC_TOTAL_SIZE         (30u * 1024u * 1024u)   /* 30 MB */
#define DLC_HEADER_SIZE        12u
#define DLC_TABLE_A_ENTRY_SZ   20u
#define DLC_TABLE_B_ENTRY_SZ   24u
#define DLC_MAIN_ENTRY_SZ      500u    /* 估计值,需 calibrate */

/* ===  Header  === */
typedef struct __attribute__((packed)) {
    uint32_t version;     /* 必须 = DLC_MAGIC_VERSION */
    uint32_t count_a;     /* 必须 = DLC_TABLE_A_COUNT */
    uint32_t reserved;    /* 必须 = 0 */
} dlc_header_t;

/* ===  Table A  - 概率/掉率曲线  === */
typedef struct __attribute__((packed)) {
    float    f1;          /* 主参数(随 id 递减) */
    int32_t  pad1;        /* 必须 = 0 */
    int32_t  pad2;        /* 必须 = 0 */
    float    f2;          /* 次参数(随 id 递增) */
    uint32_t id;          /* 1..57 */
} dlc_table_a_t;

/* ===  Table B  - ± 对称参数表  === */
typedef struct __attribute__((packed)) {
    uint32_t id;
    float    neg;
    float    pos;
    int32_t  pad;
    float    const_1001;  /* 期望值 = 1.001f */
    uint32_t next_id;
} dlc_table_b_t;

/* ===  Main Body 单条(简化)  === */
typedef struct __attribute__((packed)) {
    uint32_t id;          /* 偏移 0 */
    float    value;       /* 偏移 4 */
    uint8_t  pad[492];    /* 偏移 8..499,填 0 */
} dlc_main_entry_t;

/* ===  运行时配置(内存镜像)  === */
typedef struct {
    dlc_header_t       header;
    dlc_table_a_t      table_a[DLC_TABLE_A_COUNT];
    dlc_table_b_t      table_b[DLC_TABLE_B_COUNT];

    uint8_t*           main_body;        /* mmap 整个文件 */
    size_t             main_body_size;   /* = DLC_TOTAL_SIZE */

    uint32_t           main_count;       /* Main Body 实际非零条目数 */
} dlc_config_t;

/* ===  API  === */
int  dlc_config_load(const char* path, dlc_config_t* cfg);
void dlc_config_free(dlc_config_t* cfg);

/* 查表:id 越界返回 -1 */
float dlc_lookup_table_a(uint32_t id);
float dlc_lookup_table_b_neg(uint32_t id);
float dlc_lookup_table_b_pos(uint32_t id);

float dlc_lookup_main(uint32_t id);

int  dlc_is_ready(void);
const dlc_config_t* dlc_get_config(void);

#endif /* DLC_CONFIG_H */