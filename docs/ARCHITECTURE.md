# 架构设计 (ARCHITECTURE)

## 1. 系统总览

```
┌─────────────────────────────────────────────────────────────────┐
│                     游戏服进程 gs (Linux 32-bit)                │
│                                                                 │
│   ┌────────────────────────────────────────────────────────┐   │
│   │  libcn_clone.so  (LD_PRELOAD 注入)                     │   │
│   │                                                        │   │
│   │   __attribute__((constructor))                         │   │
│   │   libcn_clone_init()                                   │   │
│   │     │                                                  │   │
│   │     ├──► keycheck_verify()    ── (可选,默认空)        │   │
│   │     ├──► dlc_config_load()    ── 读 libdlc.data        │   │
│   │     ├──► resource_load()      ── 读 + 解密 resource    │   │
│   │     └──► install_all_hooks()  ── dlsym + 符号覆盖      │   │
│   │                                                        │   │
│   │   ┌──────────────────────────────────────┐             │   │
│   │   │ 钩子函数(同名覆盖 object_interface):│             │   │
│   │   │   UpdateHPMP                         │             │   │
│   │   │   SetCoolDown                        │             │   │
│   │   │   TestCoolDown                       │             │   │
│   │   │   ModifySkillPoint                   │             │   │
│   │   │   SendClientMsgSkillCasting          │             │   │
│   │   │   Resurrect                          │             │   │
│   │   │   UpdateAllProp                      │             │   │
│   │   │   GetPos / GetSelfID                 │             │   │
│   │   └──────────────────────────────────────┘             │   │
│   │   钩子内部:                                            │   │
│   │     dlsym(RTLD_NEXT, "原函数") → 保存真函数            │   │
│   │     dlc_lookup_xxx()        → 查参数表                 │   │
│   │     修改参数/逻辑 → 调真函数                           │   │
│   └────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
         ▲                                            ▲
         │ LD_PRELOAD 环境变量                         │ 读
         │                                            │
   ┌─────────────────────┐              ┌──────────────────────────┐
   │  启动命令           │              │  部署到 gamed/            │
   │  LD_PRELOAD=./libcn_│              │   libcn_clone.so          │
   │  clone.so ./gs ...  │              │   libdlc.data             │
   └─────────────────────┘              │   resource.data           │
                                        └──────────────────────────┘
                                                     ▲
                                                     │ 写(部署前)
                                        ┌──────────────────────────┐
                                        │  ForDataEdit 编辑器       │
                                        │  (Python + tkinter)       │
                                        │   - 读 res/*.data 源文件  │
                                        │  - 编辑参数表             │
                                        │  - 生成 libdlc.data       │
                                        │  - 加密 + 切片打包        │
                                        │    resource.data          │
                                        └──────────────────────────┘
                                                     ▲
                                        ┌──────────────────────────┐
                                        │  res/                    │
                                        │   aipolicy.data          │
                                        │   elements.data          │
                                        │   gshop.data             │
                                        │   tasks.data             │
                                        └──────────────────────────┘
```

## 2. 钩子实现原理

### 2.1 LD_PRELOAD 符号覆盖(首选)

LD_PRELOAD 注入的 `.so` 中定义的全局符号,会被动态链接器**优先**解析,盖过原共享库中的同名符号。游戏代码里如果通过 PLT/GOT 间接调用这些函数,就会调到我们的实现里。

**前提**:游戏代码不能 `-Bsymbolic-functions`,且调用要经过 GOT(动态链接的共享库默认行为)。

实现:
```c
// hooks.c
typedef int (*UpdateHPMP_fn)(void*, int, int);
static UpdateHPMP_fn real_UpdateHPMP = NULL;

int UpdateHPMP(void* self, int hp_delta, int mp_delta) {
    if (!real_UpdateHPMP) {
        real_UpdateHPMP = (UpdateHPMP_fn)dlsym(RTLD_NEXT, "UpdateHPMP");
    }
    // 业务逻辑
    int new_hp = hp_delta * (int)dlc_lookup_table_a(1);
    return real_UpdateHPMP(self, new_hp, mp_delta);
}
```

### 2.2 GOT 硬改(回退方案)

如果游戏是 `-Bsymbolic` 链接或直接 inline 调用,符号覆盖无效。需要按原报告做法:
1. `dlsym(RTLD_DEFAULT, "funcname")` 拿到当前地址
2. 反汇编入口拿到 GOT/PLT 偏移
3. `mprotect` 改页面权限为 RW
4. 把 GOT 槽位内容改成我们函数地址

本克隆版**默认走 2.1**,把 2.2 的脚手架留 `install_hooks_via_got()` 备用(待补)。

## 3. 数据流

| 阶段 | 动作 | 输入 → 输出 |
|------|------|-----------|
| 编辑 | 运营改 `res/*.data` 源文件 | res/*.data |
| 编译 | `ForDataEdit` 点"生成" | res/*.data → libdlc.data + resource.data |
| 部署 | 把三个 `.so`/`.data` 拷到 `gamed/` | libcn_clone.so + libdlc.data + resource.data |
| 启动 | LD_PRELOAD 注入 `_init` | 读 .data + 装钩 |
| 运行 | 钩子按参数表改行为 | 改写 HP/MP/冷却/掉率 |

## 4. 模块职责

| 模块 | 文件 | 职责 |
|------|------|------|
| 入口 | `main.c` | `_init` 构造、调用各模块初始化 |
| 钩子 | `hooks.c/.h` | 钩子实现 + `install_all_hooks()` |
| 配置 | `config.c/.h` | `libdlc.data` 解析 + 查表 API |
| 资源 | `resource.c/.h` | `resource.data` 加解密 + 切片 API |
| KEY | `keycheck.c/.h` | KEY 校验 hook 点(默认 no-op) |

## 5. 扩展点

- **加钩子**:`hooks.h` 加声明 + `hooks.c` 加同名函数实现 + `install_all_hooks()` 触发 lazy 解析
- **加参数**:在 `config.h` 加表项,在 `lookup_xxx()` 加分支
- **加资源切片**:在 `resource.c` 的 section 表加一行
- **接 KEY**:`keycheck.c` 替换 `keycheck_verify()` 内部

## 7. 风险与约束

- 不同 PW 版本的 `object_interface` 符号命名/签名差异巨大,需 calibrate
- 直接覆写 HP/MP 可能导致客户端/服务端校验不一致(差值过大)
- 加密资源是简单 AES,抗不了主动逆向(够阻挡 90% 玩家)
- KEY 校验默认空,运营自接