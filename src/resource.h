/* resource.h - resource.data 加解密 + 切片 */
#ifndef DLC_RESOURCE_H
#define DLC_RESOURCE_H

#include <stdint.h>
#include <stddef.h>

/* 资源节 */
typedef struct {
    char     name[64];
    uint8_t* data;
    uint32_t len;
} resource_section_t;

int  resource_load(const char* path);
void resource_unload(void);

/* 按名查找切片(返回只读指针,失败 NULL) */
const resource_section_t* resource_find(const char* name);

int  resource_is_ready(void);

#endif