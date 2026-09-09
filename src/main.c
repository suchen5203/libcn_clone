/* main.c - LD_PRELOAD 入口 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dlfcn.h>

#include "config.h"
#include "hooks.h"
#include "resource.h"
#include "keycheck.h"

/* ===  全局  === */
static int g_inited = 0;

static void log_init(const char* tag, const char* fmt, ...) {
    fprintf(stderr, "[libcn_clone:%s] ", tag);
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

/* ===  _init / __attribute__((constructor))  === */
/* 加载顺序:LD_PRELOAD → _init → 后续动态库 */
__attribute__((constructor))
static void libcn_clone_init(void) {
    if (g_inited) return;

    log_init("init", "libcn_clone.so loading...");

    /* 1) KEY 校验 */
    if (keycheck_verify() != 0) {
        log_init("init", "KEY verify failed, abort");
        _exit(582);  /* 沿用原版 KEYErrCode 582 */
    }

    /* 2) 加载 libdlc.data */
    const char* dlc_path = getenv("LIBCN_DLC");
    if (!dlc_path) dlc_path = "./libdlc.data";

    dlc_config_t* cfg = (dlc_config_t*)calloc(1, sizeof(dlc_config_t));
    if (!cfg) {
        log_init("init", "alloc cfg failed");
        _exit(1);
    }
    if (dlc_config_load(dlc_path, cfg) != 0) {
        log_init("init", "load %s failed", dlc_path);
        _exit(2);
    }
    /* cfg 指针交由全局 cfg 持有,不再 free */

    /* 3) 加载 resource.data(解密 + 切片) */
    const char* res_path = getenv("LIBCN_RES");
    if (!res_path) res_path = "./resource.data";

    if (resource_load(res_path) != 0) {
        log_init("init", "load %s failed", res_path);
        _exit(3);
    }

    /* 4) 装钩 */
    install_all_hooks();

    /* 5) 覆写 system()(对应原版导出覆盖) */
    /* 占位:此处可挂自定义 system 拦截,如禁用某些危险指令 */

    g_inited = 1;
    log_init("init", "ready (ver=%u)",
             dlc_get_config()->header.version);
}

/* ===  _fini  === */
__attribute__((destructor))
static void libcn_clone_fini(void) {
    if (!g_inited) return;
    log_init("fini", "unloading");
    dlc_config_free((dlc_config_t*)dlc_get_config());
    resource_unload();
    g_inited = 0;
}

/* ===  导出覆盖 system()(可选,默认透传)  === */
#include <unistd.h>
#include <sys/wait.h>

static int (*real_system)(const char*) = NULL;

int system(const char* cmd) {
    if (!real_system) {
        real_system = (int (*)(const char*))dlsym(RTLD_NEXT, "system");
        if (!real_system) return -1;
    }
    /* TODO: 加入你的 system() 管控策略
     * 例如:黑名单(rm -rf /)、审计日志、限频 */
    return real_system(cmd);
}