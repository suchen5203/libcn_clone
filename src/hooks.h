/* hooks.h - 钩子声明(覆盖 object_interface 系列)
 *
 * 命名约定:本文件对外暴露的符号名是 *简单名* (如 SetCoolDown),
 * 但实现侧 hooks.c 用宏把他们映射成 *mangled C++ 名*
 * (如 _ZN16object_interface11SetCoolDownEti),
 * 以便 LD_PRELOAD 在 gs 的 .dynsym 里匹配上。
 *
 * 签名按 docs/REVERSE_NOTES.md §1 校正过:
 *   SetCoolDown / TestCoolDown: unsigned short (GCC mangling 't')
 *
 * 真实调用约定 thiscall:this 经 ecx,参数 cdecl 在栈上。
 * GCC 编译时自动按 thiscall 生成 __attribute__((thiscall)) 标记。
 *
 * 若目标 -Bsymbolic 链接,需走 GOT 硬改(见 install_hooks_via_got)。
 */
#ifndef DLC_HOOKS_H
#define DLC_HOOKS_H

#include <stdint.h>

/* -------- 钩子声明(简单名,实现侧宏化) -------- */

/* Heal:  XID 是引用,实际 hook 这个不太好用(参数是 const XID&),
 *       改 delta 的钩点建议走 _ZN16object_interface11InjectManaEi(蓝)
 *       和 _ZN16object_interface4HealERK3XIDj(红),不直接走 UpdateHPMP(那个不存在)。
 *       这里给出兼容声明,接口里不用 UpdateHPMP。 */
int   InjectMana(void* self, int delta);   /* 注入蓝,实际应用率/封顶 */
int   DrainMana(void* self, int delta);   /* 抽蓝 */
int   Heal(void* self, const void* xid, unsigned int hp);  /* 加血 */

/* 主钩子 */
int   SetCoolDown(void* self, unsigned short skill_id, int cd_ms);
int   TestCoolDown(void* self, unsigned short skill_id);
int   ModifySkillPoint(void* self, int delta);
int   SendClientMsgSkillCasting(void* self, const void* xid, int skill_id, unsigned char flags);
int   Resurrect(void* self, float hp_pct, float mp_pct, float vigor_pct);
int   UpdateAllProp(void* self);
int   GetPos(void* self);
int   GetSelfID(void* self);

/* 触发 lazy 解析所有钩子对应的真函数(RTLD_NEXT) */
void  install_all_hooks(void);

/* GOT 硬改回退(占位,版本相关,需 calibrate 后实现) */
void  install_hooks_via_got(void);

#endif