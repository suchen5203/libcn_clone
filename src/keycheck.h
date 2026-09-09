/* keycheck.h - KEY 校验 hook 点(默认空,留给你接) */
#ifndef DLC_KEYCHECK_H
#define DLC_KEYCHECK_H

/* 返回 0 = 通过,非 0 = 失败(main.c 会 _exit(582)) */
int keycheck_verify(void);

#endif