# REVERSE_NOTES.md — `gs` ELF 反汇编笔记

> **来源**:`E:\书生插件\clone\gs`(385 MB,32-bit x86 ELF EXEC)
> **目的**:为 `libcn_clone.so` 提供精确的"钩子目标"模型,使得 LD_PRELOAD 覆盖函数时**函数签名一一对齐**
> **工具**:Python 3.13 + capstone 5.0.9(本机无 objdump/nm/readelf)
> **范围**:本批只反汇编了 4 个目标入口;后续若需要其他函数(ModifySkillPoint / DrainMana / SendClientMsgSkillCasting 等)按同样方法追加

---

## 0. ELF 概览

| 项 | 值 |
|---|---|
| 入口 vaddr | `0x805e750` |
| 类型 | `EXEC`(非 PIE) |
| LOAD 段 | 2 段(RX .text `0x8048000`→`0x8923000`,RW .data `0x8923000`→`0x8986000`) |
| `.dynsym` | 1169 个动态符号,18,704 B |
| `.symtab` | 全符号表,~47K 条,758 KB |
| `.dynstr` | 52,946 B |
| 字节序 | LSB,32-bit |
| C++ 调用约定 | **thiscall**(`this` 经 `ecx`,参数从栈 cdecl) |

**地址映射关键**:
- 任何虚拟地址 `v` → 文件偏移 `v - 0x8048000`(第 2 LOAD 段调整后才用 +0x8db000)
- 后续如果要做 patch,这是基础

---

## 1. 真实签名校正(GCC Itanium ABI mangling 规则)

`.dynsym` 里所有目标符号都是 **mangled 名**;demangler 给出的字符串会因 demangler 版本不同导致精度差异。下面按 **GCC 4.x 的 mangling** 规则手动解:

| mangled 名 | demangler 给 | 真实类型 |
|---|---|---|
| `_ZN16object_interface11SetCoolDownEti` | `SetCoolDown(unsigned int, int)` ❌ | `SetCoolDown(unsigned short, int)` ✅(因为 `t` = unsigned short,不是 unsigned int;反汇编里变量存成 `word ptr` 印证) |
| `_ZN16object_interface12TestCoolDownEt` | `TestCoolDown(unsigned int)` ❌ | `TestCoolDown(unsigned short)` ✅(`t` = unsigned short) |
| `_ZN16object_interface11SetCoolDownEti` | 同上 | 唯一键值类型 pair: `(skill_id: u16, cooldown_ms: i32)` |
| `_ZN16object_interface25SendClientMsgSkillCastingERK3XIDith` | `(XID const&, int, unsigned char)` ✅ | 已正确(`K3XID` = `const&` + 长度 3 模板实参 `XID`,`i` = int,`t` = unsigned short,**末尾 h = unsigned char**) |
| `_ZN16object_interface16ModifySkillPointEi` | `(int)` ✅ |  |
| `_ZN16object_interface9ResurrectEfff` | `(float, float, float)` ✅ |  |
| `_ZN16object_interface4HealERK3XIDj` | `(XID const&, unsigned int)` ✅ |  |

> **结论**:SetCoolDown/TestCoolDown 的参数类型需要在我们 `hooks.h` 里改成 `unsigned short`,否则栈/寄存器对齐不咬。

---

## 2. 4 个目标入口反汇编

### 2.1 `object_interface::Heal(XID const&, unsigned int)` — `0x8178470`,30 B

```asm
8178470: 55                push     ebp
8178471: 89e5              mov      ebp, esp
8178473: 83ec08            sub      esp, 8
8178476: 83ec04            sub      esp, 4
8178479: ff7510            push     dword ptr [ebp + 0x10]   ; arg2: hp_amount (u32)
817847c: ff750c            push     dword ptr [ebp + 0xc]    ; arg1: &XID (const&)
817847f: 8b4508            mov      eax, dword ptr [ebp + 8] ; eax = this
8178482: ff30              push     dword ptr [eax]          ; push *this (first dword of XID)
8178484: e85b680000        call     0x817ece4                ; tail-call inner func
8178489: 83c410            add      esp, 0x10
817848c: c9                leave
817848d: c3                ret
```

**调用栈**:`this(arg0)` `[ebp+8]`,`&XID(arg1)` `[ebp+0xc]`,`hp(arg2)` `[ebp+0x10]`。
**尾部 callee**:`void inner(XID_struct_dword, XID_ref, hp)`(@0x817ece4)。

### 2.2 `Heal_callee` — `0x817ece4`(Heal 内部实现,跨函数)

```asm
817ece4: 55                push     ebp
817ece5: 89e5              mov      ebp, esp
817ece7: 83ec18            sub      esp, 0x18
817ecea: 83ec0c            sub      esp, 0xc
817eced: 8b4508            mov      eax, [ebp + 8]           ; eax = arg1 (XID_struct first dword)
817ecf0: ff7008            push     [eax + 8]                ; push XID+8
817ecf3: e8eaa5eeff        call     0x80692e2                ; 验证/查找函数
817ecf8: 83c410            add      esp, 0x10
817ecfb: 84c0              test     al, al
817ecfd: 7405              je       0x817ed04                ; 失败 → jmp
817ecff: e9b2000000        jmp      0x817edb6                ; 成功 → 跳过赋值
817ed04: 8b5508            mov      edx, [ebp + 8]           ; edx = XID_struct ptr
817ed07: 8b4510            mov      eax, [ebp + 0x10]        ; eax = hp
817ed0a: 038268020000      add      eax, [edx + 0x268]       ; eax = HP_cur + delta
817ed10: 99                cdq                              ; edx:eax sign-extend (for 64-bit store)
817ed11: 8945f8            mov      [ebp - 8], eax
817ed14: 8955fc            mov      [ebp - 4], edx
817ed17: 8b4d08            mov      ecx, [ebp + 8]           ; ecx = XID_struct ptr
817ed1a: 8b5508            mov      edx, [ebp + 8]
817ed1d: 8b4510            mov      eax, [ebp + 0x10]
817ed20: 038268020000      add      eax, [edx + 0x268]       ; HP_cur + delta
817ed26: 898168020000      mov      [ecx + 0x268], eax       ; **写回 HP 字段(偏移 +0x268)**
817ed2c: 83ec0c            sub      esp, 0xc
817ed2f: ff7508            push     [ebp + 8]
```

**关键事实**:
- **`XID_struct` 的 HP 字段位于偏移 `+0x268`**(32-bit,直接累加)
- 写回 HP 之前会先调 `0x80692e2`(推测是"对象存活/有效检查"),返回值非 0 时跳过写
- 累加是 **in-place**:`HP_cur += delta`,**无写后通知**(被我们的钩子包住时,我们能拦住的就是 `delta` 这个值)

### 2.3 `object_interface::SetCoolDown(unsigned short, int)` — `0x817c0a8`,51 B

```asm
817c0a8: 55                push     ebp
817c0a9: 89e5              mov      ebp, esp
817c0ab: 83ec08            sub      esp, 8
817c0ae: 8b450c            mov      eax, [ebp + 0xc]        ; eax = cooldown
817c0b1: 668945fe          mov      word ptr [ebp - 2], ax  ; **存成 word(16-bit)**
817c0b5: 83ec04            sub      esp, 4
817c0b8: 8b4508            mov      eax, [ebp + 8]          ; eax = this
817c0bb: 8b00              mov      eax, [eax]             ; eax = vtable ptr
817c0bd: 8b10              mov      edx, [eax]             ; edx = vtable[0]
817c0bf: 81c238010000      add      edx, 0x138             ; **edx = vtable + 0x138 字节**
817c0c5: ff7510            push     [ebp + 0x10]           ; push skill_id(u16)
817c0c8: 0fb745fe          movzx    eax, word ptr [ebp - 2]; eax = zero-ext 16-bit cooldown
817c0cc: 50                push     eax
817c0cd: 8b4508            mov      eax, [ebp + 8]         ; eax = this
817c0d0: ff30              push     [eax]                  ; push XID_struct first dword
817c0d2: 8b02              mov      eax, [edx]             ; eax = **虚函数指针**
817c0d4: ffd0              call     eax                    ; 间接 call
817c0d6: 83c410            add      esp, 0x10
817c0d9: c9                leave
817c0da: c3                ret
```

**关键事实**:
- `cooldown` 参数在函数体内**被截成 16-bit**(`mov word ptr`)
- 真正干活的逻辑是 **虚函数**(vtable 偏移 `0x138`/4 = entry `0x4e`),**不是 this 函数本身**
- 也就是说,**我们钩 SetCoolDown 拦下来后,如果直接 return 原值,实际虚函数照跑**;要在我们的钩子里"修改 cooldown"必须 **改 cooldown 参数再 call 真实实现**
- 调虚函数时实际传 3 个参:`(XID_struct_dword, skill_id, cooldown)`,**对应一个 `(object_interface*, u16, u32)` 接口**

> **钩子策略**:LD_PRELOAD 覆盖 `_ZN16object_interface11SetCoolDownEt` 后,可根据 `cooldown` 走"概率判定 → 修改 → 调原函数"三步。原函数可 `dlsym(RTLD_NEXT, ...)` 取地址。

### 2.4 `object_interface::Resurrect(float, float, float)` — **两个重载**

#### 2.4.1 重载 A(37 B,wrapper)— `0x8179310`

```asm
8179310: 55                push     ebp
8179311: 89e5              mov      ebp, esp
8179313: 83ec08            sub      esp, 8
8179316: b8cdcccc3d        mov      eax, 0x3dcccccd        ; 0.1f (IEEE 754)
817931b: 50                push     eax                    ; arg3 = 0.1f (mp)
817931c: b8cdcccc3d        mov      eax, 0x3dcccccd        ; 0.1f
8179321: 50                push     eax                    ; arg2 = 0.1f (hp)
8179322: ff750c            push     [ebp + 0xc]            ; arg1 (?)
8179325: ff7508            push     [ebp + 8]              ; this
8179328: e8b3ffffff        call     0x81792e0              ; 调主版本(47B)
817932d: 83c410            add      esp, 0x10
8179330: 0fb6c0            movzx    eax, al                ; 返回 bool
8179333: c9                leave
8179334: c3                ret
```

**关键事实**:37B 版本是 wrapper,**默认传 `0.1f, 0.1f` 作 HP/MP**。也就是说,正常 "Resurrect()" 调用实际把 HP/MP 都回 10%。

#### 2.4.2 重载 B(47 B,主版本)— `0x81792e0`

```asm
81792e0: 55                push     ebp
81792e1: 89e5              mov      ebp, esp
81792e3: 83ec08            sub      esp, 8
81792e6: 8b4508            mov      eax, [ebp + 8]          ; eax = this
81792e9: 8b00              mov      eax, [eax]             ; eax = vtable
81792eb: 8b10              mov      edx, [eax]             ; edx = vtable[0]
81792ed: 81c2d8000000      add      edx, 0xd8              ; **vtable + 0xd8 字节**(entry 0x36)
81792f3: ff7514            push     [ebp + 0x14]           ; arg3
81792f6: ff7510            push     [ebp + 0x10]           ; arg2
81792f9: ff750c            push     [ebp + 0xc]            ; arg1
81792fc: 8b4508            mov      eax, [ebp + 8]
81792ff: ff30              push     [eax]                  ; XID_struct first dword
8179301: 8b02              mov      eax, [edx]             ; 虚函数指针
8179303: ffd0              call     eax
8179305: 83c410            add      esp, 0x10
8179308: b801000000        mov      eax, 1                 ; 总是返回 1
817930d: c9                leave
817930e: c3                ret
```

**关键事实**:
- 主版本也是**间接调用虚函数**(vtable + `0xd8` 字节,entry `0x36`)
- 返回值固定为 `1`(成功)
- 实际"复活成功"由虚函数决定

---

## 3. 对 `libcn_clone` 的硬约束

| 维度 | 约束 |
|---|---|
| **符号名匹配** | 必须用 mangled 名(`_ZN16object_interface11SetCoolDownEti` 这种),简单名匹配失败 |
| **签名对齐** | SetCoolDown/TestCoolDown 必须是 `unsigned short`(不是 `unsigned int`!) |
| **thiscall** | `this` 经 `ecx`;函数声明不显式写,但 GCC 编译时自动按 thiscall 生成(可手动用 `__attribute__((thiscall))` 确认) |
| **间接调用虚函数** | SetCoolDown / Resurrect 实际干活的是 **vtable 间接 call**,我们拦下来必须 `dlsym(RTLD_NEXT,...)` 拿到原符号再调,不能跳过(否则整个钩子失效) |
| **HP 字段偏移** | `XID_struct + 0x268`(32-bit signed,直接累加);想拦 HP 修改,可以拦 Heal 的内部 callee `0x817ece4` 改 `delta` 参数 |
| **EXPORT 顺序** | hooks.c 里 `__attribute__((visibility("default")))` 必须每个目标符号都标,否则 LD_PRELOAD 看不见 |

---

## 4. `libcn_clone` 需要更新的位置

| 文件 | 改动 |
|---|---|
| `src/hooks.h` | 1. SetCoolDown/TestCoolDown 参数类型改成 `unsigned short`<br>2. 所有符号都用 mangled 名(顶部宏定义 `_M(s)` 包裹) |
| `src/hooks.c` | 1. 每个覆盖函数必须 `__attribute__((visibility("default")))`<br>2. 需要 forward 调用时 `dlsym(RTLD_NEXT, "_ZN...EXi")`<br>3. SetCoolDown 的钩子要"先改 cooldown 再调原函数",不要 return |
| `src/config.c` | 概率/冷却调整的判定走 `libdlc.data` 的 hook table;改 `delta` 时如果是 u16 类型要 cast 正确 |
| `src/main.c` | 不动(继续走 `_init` / `dlsym` / `mprotect` 流程) |

---

## 5. 下一步候选

| 优先级 | 项 | 工具 |
|---|---|---|
| 🔴 高 | 反汇编 `ModifySkillPoint` / `DrainMana` / `InjectMana` | 同上 |
| 🔴 高 | 反汇编 `SendClientMsgSkillCasting`(37B+,签名复杂) | 同上 |
| 🟡 中 | 找 `PlayerWrapper`(玩家类包装层)的 vtable 起始地址 | nm .symtab 搜索 |
| 🟡 中 | 找 PW 版本字符串(`strings -el` 或 grep GNET) | grep 二进制 |
| 🟢 低 | 反汇编 `fopen` libdlc.so 真正要 patch 的 syscall | objdump 整段 |