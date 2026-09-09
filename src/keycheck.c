/* keycheck.c - KEY 校验占位实现
 *
 * 默认 no-op,直接通过。
 *
 * 接法建议:
 *   1) 本地 HASH 比对:读 ./libcn.key,与 .so 内硬编码 HASH 比对
 *   2) 远程校验:HTTP 调用你的授权服务(注意服务器可能无法联网)
 *   3) RSA 签名:.so 内嵌公钥,key 文件带签名
 *   4) 时间窗口:key 含有效期,过期失效
 *
 * 反破解强度参考:
 *   - 任何客户端都能被 dump,所以密钥/HASH 不能只在 .so 里
 *   - 服务器侧校验(远程 HTTP)是最强的
 */
#include "keycheck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ===  示例 1:本地 HASH 简单比对(占位)  === */
static int check_local_hash(const char* path) {
    /* TODO: 你的 HASH 校验逻辑
     * 伪代码:
     *   read 16B from path
     *   compute SHA256
     *   compare with embedded expected hash
     *   return 0 if match
     */
    (void)path;
    return 0;  /* 默认通过 */
}

/* ===  示例 2:远程 HTTP 校验(占位)  === */
static int check_remote(const char* server_id) {
    /* TODO: curl_easy_perform 检查授权
     * 注意:游戏服可能无外网,要兼容离线场景
     */
    (void)server_id;
    return 0;
}

int keycheck_verify(void) {
    /* 默认全通过,留给运营方接 */
    return 0;

    /* 如果你要启用,把上面 return 0 注释掉,解开下面的:
     *
     * const char* key_path = getenv("LIBCN_KEY_PATH");
     * if (!key_path) key_path = "./libcn.key";
     *
     * if (access(key_path, R_OK) != 0) {
     *     fprintf(stderr, "[keycheck] no key file at %s\n", key_path);
     *     return 1;
     * }
     *
     * if (check_local_hash(key_path) != 0) {
     *     fprintf(stderr, "[keycheck] local hash mismatch\n");
     *     return 2;
     * }
     *
     * const char* sid = getenv("LIBCN_SERVER_ID");
     * if (sid && check_remote(sid) != 0) {
     *     fprintf(stderr, "[keycheck] remote auth failed\n");
     *     return 3;
     * }
     *
     * return 0;
     */
}