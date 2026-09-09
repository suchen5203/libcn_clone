/* hooks.c - 钩子实现(覆盖 object_interface 系列)
 *
 * ============================================================
 *  重要:calling convention & ABI 兼容性
 * ============================================================
 *  GS 用 GCC 编译 C++,thiscall 调用约定:
 *    1) caller 把 self 作为隐式第一参,按 cdecl 顺序 push 栈
 *    2) 同时把 self 复制到 ecx(thiscall 的特征)
 *    3) 然后 call
 *
 *  我们用 C 写 hook(编译为 cdecl):
 *    - 函数声明按 cdecl,把 self 作为显式第一个参数
 *    - 栈上 self 在 [esp+4],与 caller 推的一致 → 直接用
 *    - 调 *real*(gs 原函数)时,要按 thiscall 调,inline asm 把 self 放入 ecx
 *
 *  覆盖符号用 mangled 名(asm alias),这样 LD_PRELOAD 在 gs 的
 *  .dynsym 里匹配上;源码可读性靠 C 名称。
 *
 *  反汇编依据: docs/REVERSE_NOTES.md
 * ============================================================ */

#define _GNU_SOURCE
#include "hooks.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

/* ---------- mangled name 映射 ----------
 * C 源里函数叫 SetCoolDown,但符号名是 _ZN16object_interface11SetCoolDownEti。
 * 这样源码可读,链接符号又是 GS 期望的。 */
#define MANGLED(name, m) \
    __asm__(".global " #m "\n" \
            ".type   " #m ", @function\n" \
            #m " = " #name "\n")

/* ---------- thiscall 调用 real 函数 ----------
 * GCC 默认按 cdecl 编译 C,我们要模拟 thiscall 调 GS 的 C++ 函数。
 * 每个 arity 给一个 helper。*/

static inline int tc_invoke_0(void* fn, void* self) {
    int ret;
    asm volatile("mov %1, %%ecx\n\t"
                 "call *%2"
                 : "=a"(ret)
                 : "r"(self), "r"(fn)
                 : "ecx", "edx", "memory");
    return ret;
}

static inline int tc_invoke_1(void* fn, void* self, int a1) {
    int ret;
    asm volatile("push %2\n\t"
                 "mov %1, %%ecx\n\t"
                 "call *%3\n\t"
                 "add $4, %%esp"
                 : "=a"(ret)
                 : "r"(self), "r"(a1), "r"(fn)
                 : "ecx", "edx", "memory");
    return ret;
}

static inline int tc_invoke_2(void* fn, void* self, int a1, int a2) {
    int ret;
    asm volatile("push %3\n\t"
                 "push %2\n\t"
                 "mov %1, %%ecx\n\t"
                 "call *%4\n\t"
                 "add $8, %%esp"
                 : "=a"(ret)
                 : "r"(self), "r"(a1), "r"(a2), "r"(fn)
                 : "ecx", "edx", "memory");
    return ret;
}

static inline int tc_invoke_3(void* fn, void* self, int a1, int a2, int a3) {
    int ret;
    asm volatile("push %4\n\t"
                 "push %3\n\t"
                 "push %2\n\t"
                 "mov %1, %%ecx\n\t"
                 "call *%5\n\t"
                 "add $12, %%esp"
                 : "=a"(ret)
                 : "r"(self), "r"(a1), "r"(a2), "r"(a3), "r"(fn)
                 : "ecx", "edx", "memory");
    return ret;
}

static inline int tc_invoke_4(void* fn, void* self, int a1, int a2, int a3, int a4) {
    int ret;
    asm volatile("push %5\n\t"
                 "push %4\n\t"
                 "push %3\n\t"
                 "push %2\n\t"
                 "mov %1, %%ecx\n\t"
                 "call *%6\n\t"
                 "add $16, %%esp"
                 : "=a"(ret)
                 : "r"(self), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(fn)
                 : "ecx", "edx", "memory");
    return ret;
}

/* ---------- dlsym helper:解析 real 函数 ----------
 * 用 RTLD_NEXT 拿 GS 原函数地址。注意符号名是 mangled 名。 */

#define RESOLVE_REAL_MANGLED(simple_name, mangled_name, fn_t, real_var) \
    static fn_t real_var = NULL; \
    if (!real_var) { \
        real_var = (fn_t)dlsym(RTLD_NEXT, mangled_name); \
        if (!real_var) { \
            fprintf(stderr, "[libcn_clone:hooks] " mangled_name " not found\n"); \
            return -1; \
        } \
    }

/* log helper */
static inline void log_call(const char* fn, void* self) {
    if (getenv("LIBCN_CLONE_LOG"))
        fprintf(stderr, "[libcn_clone:hook] %s self=%p\n", fn, self);
}

/* ============================================================
 *  以下每个函数都按 cdecl 声明(thiscall 兼容,见顶部说明)
 *  函数体用 mangled alias 让符号名匹配 GS 的 .dynsym
 * ============================================================ */

/* ---------- InjectMana(int delta) ----------
 * mangled: _ZN16object_interface10InjectManaEi
 * 签名 (object_interface*, int) */
typedef int (*InjectMana_fn)(void*, int);
MANGLED(InjectMana_hook, _ZN16object_interface10InjectManaEi)
int InjectMana_hook(void* self, int delta) {
    RESOLVE_REAL_MANGLED(InjectMana, "_ZN16object_interface10InjectManaEi",
                         InjectMana_fn, real_InjectMana);
    log_call("InjectMana", self);
    if (!dlc_is_ready()) return tc_invoke_1(real_InjectMana, self, delta);

    /* table_a id=2 作为 MP 倍率 */
    float mult = dlc_lookup_table_a(2);
    int new_delta = (int)(delta * mult);
    return tc_invoke_1(real_InjectMana, self, new_delta);
}

/* ---------- DrainMana(int delta) ----------
 * mangled: _ZN16object_interface9DrainManaEi */
typedef int (*DrainMana_fn)(void*, int);
MANGLED(DrainMana_hook, _ZN16object_interface9DrainManaEi)
int DrainMana_hook(void* self, int delta) {
    RESOLVE_REAL_MANGLED(DrainMana, "_ZN16object_interface9DrainManaEi",
                         DrainMana_fn, real_DrainMana);
    log_call("DrainMana", self);
    if (!dlc_is_ready()) return tc_invoke_1(real_DrainMana, self, delta);

    float mult = dlc_lookup_table_a(2);
    int new_delta = (int)(delta * mult);
    return tc_invoke_1(real_DrainMana, self, new_delta);
}

/* ---------- Heal(XID const&, unsigned int hp) ----------
 * mangled: _ZN16object_interface4HealERK3XIDj
 * 注意 XID& 通过 const ref,在栈上是 8 字节(XID 通常 8 字节结构体),
 * 我们按 ptr 处理(GCC 在栈上推 const& 就是推指针)。
 * 所以签名前两个参数实际是 (self, const XID*) */
typedef int (*Heal_fn)(void*, const void*, unsigned int);
MANGLED(Heal_hook, _ZN16object_interface4HealERK3XIDj)
int Heal_hook(void* self, const void* xid, unsigned int hp) {
    RESOLVE_REAL_MANGLED(Heal, "_ZN16object_interface4HealERK3XIDj",
                         Heal_fn, real_Heal);
    log_call("Heal", self);
    if (!dlc_is_ready()) return tc_invoke_2(real_Heal, self, (int)(intptr_t)xid, (int)hp);

    /* table_a id=1 HP 倍率 */
    float mult = dlc_lookup_table_a(1);
    unsigned int new_hp = (unsigned int)(hp * mult);
    return tc_invoke_2(real_Heal, self, (int)(intptr_t)xid, (int)new_hp);
}

/* ---------- SetCoolDown(unsigned short skill_id, int cd_ms) ----------
 * mangled: _ZN16object_interface11SetCoolDownEti
 * 签名校正:`t` = unsigned short,不是 unsigned int(见 REVERSE_NOTES §1)
 * 真正的活是 vtable 间接 call,所以我们要调 real。 */
typedef int (*SetCoolDown_fn)(void*, unsigned short, int);
MANGLED(SetCoolDown_hook, _ZN16object_interface11SetCoolDownEti)
int SetCoolDown_hook(void* self, unsigned short skill_id, int cd_ms) {
    RESOLVE_REAL_MANGLED(SetCoolDown, "_ZN16object_interface11SetCoolDownEti",
                         SetCoolDown_fn, real_SetCoolDown);
    log_call("SetCoolDown", self);

    if (!dlc_is_ready()) return tc_invoke_2(real_SetCoolDown, self, (int)skill_id, cd_ms);

    /* table_a id=3 冷却倍率(>1 加长,<1 缩短) */
    float mult = dlc_lookup_table_a(3);
    int new_cd = (int)(cd_ms * mult);
    if (new_cd < 0) new_cd = 0;
    return tc_invoke_2(real_SetCoolDown, self, (int)skill_id, new_cd);
}

/* ---------- TestCoolDown(unsigned short skill_id) ----------
 * mangled: _ZN16object_interface12TestCoolDownEt
 * 直接透传,不改判定结果(改了会跟客户端不同步)。 */
typedef int (*TestCoolDown_fn)(void*, unsigned short);
MANGLED(TestCoolDown_hook, _ZN16object_interface12TestCoolDownEt)
int TestCoolDown_hook(void* self, unsigned short skill_id) {
    RESOLVE_REAL_MANGLED(TestCoolDown, "_ZN16object_interface12TestCoolDownEt",
                         TestCoolDown_fn, real_TestCoolDown);
    return tc_invoke_1(real_TestCoolDown, self, (int)skill_id);
}

/* ---------- ModifySkillPoint(int delta) ----------
 * mangled: _ZN16object_interface16ModifySkillPointEi */
typedef int (*ModifySkillPoint_fn)(void*, int);
MANGLED(ModifySkillPoint_hook, _ZN16object_interface16ModifySkillPointEi)
int ModifySkillPoint_hook(void* self, int delta) {
    RESOLVE_REAL_MANGLED(ModifySkillPoint, "_ZN16object_interface16ModifySkillPointEi",
                         ModifySkillPoint_fn, real_ModifySkillPoint);
    log_call("ModifySkillPoint", self);

    if (!dlc_is_ready()) return tc_invoke_1(real_ModifySkillPoint, self, delta);

    /* table_a id=4 技能点倍率 */
    float mult = dlc_lookup_table_a(4);
    int new_delta = (int)(delta * mult);
    return tc_invoke_1(real_ModifySkillPoint, self, new_delta);
}

/* ---------- SendClientMsgSkillCasting(XID const&, int skill_id, unsigned char flags) ----------
 * mangled: _ZN16object_interface25SendClientMsgSkillCastingERK3XIDith
 * 参数顺序在 cdecl 栈上是 (self, &XID, skill_id, flags)。 */
typedef int (*SendClientMsgSkillCasting_fn)(void*, const void*, int, unsigned char);
MANGLED(SendClientMsgSkillCasting_hook, _ZN16object_interface25SendClientMsgSkillCastingERK3XIDith)
int SendClientMsgSkillCasting_hook(void* self, const void* xid, int skill_id, unsigned char flags) {
    RESOLVE_REAL_MANGLED(SendClientMsgSkillCasting,
                         "_ZN16object_interface25SendClientMsgSkillCastingERK3XIDith",
                         SendClientMsgSkillCasting_fn, real_SendClientMsgSkillCasting);
    /* 这里默认透传;如果需要"技能冷却判定拦截"则可在这里走表 b 判定 */
    return tc_invoke_3(real_SendClientMsgSkillCasting, self,
                       (int)(intptr_t)xid, skill_id, (int)flags);
}

/* ---------- Resurrect(float hp, float mp, float vigor) ----------
 * mangled: _ZN16object_interface9ResurrectEfff
 * 这是 47B 主版本(原报告漏了 vigor 参数);37B 版本是 wrapper 调这个,默认传 0.1f。 */
typedef int (*Resurrect_fn)(void*, float, float, float);
MANGLED(Resurrect_hook, _ZN16object_interface9ResurrectEfff)
int Resurrect_hook(void* self, float hp, float mp, float vigor) {
    RESOLVE_REAL_MANGLED(Resurrect, "_ZN16object_interface9ResurrectEfff",
                         Resurrect_fn, real_Resurrect);
    log_call("Resurrect", self);

    /* 强制满血满蓝复活:把百分比都拉满 */
    return tc_invoke_3(real_Resurrect, self, 1.0f, 1.0f, 1.0f);
}

/* ---------- UpdateAllProp() ----------
 * mangled: _ZN16object_interface13UpdateAllPropEv */
typedef int (*UpdateAllProp_fn)(void*);
MANGLED(UpdateAllProp_hook, _ZN16object_interface13UpdateAllPropEv)
int UpdateAllProp_hook(void* self) {
    RESOLVE_REAL_MANGLED(UpdateAllProp, "_ZN16object_interface13UpdateAllPropEv",
                         UpdateAllProp_fn, real_UpdateAllProp);
    return tc_invoke_0(real_UpdateAllProp, self);
}

/* ---------- GetPos() ----------
 * mangled: _ZN16object_interface6GetPosEv
 * 反汇编显示无参,16B,可能通过 eax 返回,或 this 全局缓存位置。
 * 先按无参走,具体语义后续看 gs 哪里调它。 */
typedef int (*GetPos_fn)(void*);
MANGLED(GetPos_hook, _ZN16object_interface6GetPosEv)
int GetPos_hook(void* self) {
    RESOLVE_REAL_MANGLED(GetPos, "_ZN16object_interface6GetPosEv",
                         GetPos_fn, real_GetPos);
    return tc_invoke_0(real_GetPos, self);
}

/* ---------- GetSelfID() ----------
 * mangled: _ZN16object_interface9GetSelfIDev */
typedef int (*GetSelfID_fn)(void*);
MANGLED(GetSelfID_hook, _ZN16object_interface9GetSelfIDev)
int GetSelfID_hook(void* self) {
    RESOLVE_REAL_MANGLED(GetSelfID, "_ZN16object_interface9GetSelfIDev",
                         GetSelfID_fn, real_GetSelfID);
    return tc_invoke_0(real_GetSelfID, self);
}

/* ============================================================
 *  install_all_hooks - 提前解析所有真函数符号,提前报错
 * ============================================================ */
void install_all_hooks(void) {
    static const char* syms[] = {
        "_ZN16object_interface10InjectManaEi",
        "_ZN16object_interface9DrainManaEi",
        "_ZN16object_interface4HealERK3XIDj",
        "_ZN16object_interface11SetCoolDownEti",
        "_ZN16object_interface12TestCoolDownEt",
        "_ZN16object_interface16ModifySkillPointEi",
        "_ZN16object_interface25SendClientMsgSkillCastingERK3XIDith",
        "_ZN16object_interface9ResurrectEfff",
        "_ZN16object_interface13UpdateAllPropEv",
        "_ZN16object_interface6GetPosEv",
        "_ZN16object_interface9GetSelfIDev",
        NULL
    };
    int missing = 0;
    for (int i = 0; syms[i]; i++) {
        void* p = dlsym(RTLD_NEXT, syms[i]);
        if (!p) {
            fprintf(stderr, "[libcn_clone:hooks] missing: %s\n", syms[i]);
            missing++;
        }
    }
    if (missing) {
        fprintf(stderr,
                "[libcn_clone:hooks] WARNING: %d symbols missing, "
                "LD_PRELOAD override may not work. "
                "Try install_hooks_via_got() for -Bsymbolic binaries.\n",
                missing);
    } else {
        fprintf(stderr, "[libcn_clone:hooks] all 11 hooks resolved\n");
    }
}

/* ============================================================
 *  install_hooks_via_got - GOT 硬改回退(占位)
 * ============================================================ */
void install_hooks_via_got(void) {
    fprintf(stderr,
            "[libcn_clone:hooks] install_hooks_via_got: NOT IMPLEMENTED, "
            "calibrate against your target binary\n");
}