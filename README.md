# 书生插件 · 克隆版 (libcn_clone)

完整克隆自 `libdlc.so` 的完美世界 GNET 私服玩法插件 —— **全新源码,按你给的版本与功能定制**。

## ⚠️ 边界声明

- **不逆向原 `.so` 字节码、不抄 KEY 校验、不绕授权** —— 所有源码新写
- `libdlc.data` / `resource.data` 的二进制格式按**逆向报告复刻**(头/表 A/表 B/主体/加密),不承诺 100% 字节级兼容原版
- 默认配置:**PW 经典版 GNET** + **全套钩子**(HP/MP/冷却/技能点/掉率/复活) + **AES-128-CBC 资源加密** + **Docker/WSL 编译**

## 目录

```
clone/
├── main.c            # LD_PRELOAD 入口 (_init, install_hooks)
├── hooks.c / .h      # 钩子实现(覆盖 object_interface 系列)
├── config.c / .h     # libdlc.data 解析 + 参数表查表
├── resource.c / .h   # resource.data 加解密 + 切片
├── keycheck.c / .h   # KEY 校验(默认空,留 hook 点)
├── Makefile          # 32-bit ELF 编译(需 Linux 32 位工具链)
└── Dockerfile        # i386/ubuntu 容器化编译(Windows 友好)
docs/
├── ARCHITECTURE.md   # 系统架构
└── DATA_FORMAT.md    # libdlc.data / resource.data 格式
editor/               # Python GUI 编辑器(下个迭代补)
res/                  # aipolicy/elements/gshop/tasks 源文件
```

## 快速上手

### 路线 1:Windows 一键(自动检测 Docker Desktop / WSL)

```cmd
cd clone
build.bat
```

脚本会自动选择可用路线;都没有时会打印安装指引。

### 路线 2:Git Bash / Linux / macOS

```bash
cd clone
bash build.sh          # 自动选择 docker / podman / 本地 gcc -m32
```

### 路线 3:GitHub Actions 云端构建

推到 GitHub 后 `.github/workflows/build.yml` 自动跑,完成后在 Actions 页 Artifacts 区域下载 `libcn_clone.so`。

### 路线 4:手动(Docker)

```bash
docker build -t libcn_clone .
docker run --rm -v $(pwd):/build libcn_clone make check
```

### 手动(Linux 32 位工具链)

```bash
sudo apt install -y gcc-multilib libssl-dev make
make USE_OPENSSL=1
make check
./smoke_test.sh
```

### 部署

```bash
cp libcn_clone.so libdlc.data resource.data /path/to/gamed/
cd /path/to/gamed && \
  LD_PRELOAD=./libcn_clone.so ./gs gs.conf gmserver.conf gsalias.conf &
```

## 版本待定项

| 项 | 默认假设 | 你需要确认 |
|----|---------|----------|
| PW 版本 | 经典 GNET (2010 前后) | 具体版本号 |
| `object_interface` 符号表 | 按报告里的命名 | 是否完全一致 |
| KEY 算法 | 不启用 | 是否需要,用什么算法 |
| 资源加密 | AES-128-CBC + 密钥 `0xDEADBEEF...` | 密钥来源 |

## 状态

- [x] 工程骨架 + 架构文档
- [x] libdlc.data 解析 + 查表
- [x] LD_PRELOAD 入口 + 钩子框架(1 个示例:HP 倍率)
- [x] resource.data 加解密骨架
- [x] KEY 校验 hook 点
- [ ] 编辑器 GUI(Python tkinter)
- [ ] res/ 源数据生成器
- [ ] 针对具体版本符号表的 calibrate 脚本