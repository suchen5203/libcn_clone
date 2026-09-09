# GitHub 上传与 Actions 构建指南

> 把 `libcn_clone` 推到 GitHub,让 GitHub Actions 自动编译 `libcn_clone.so`,
> 你 5 分钟后在 Actions 页 Artifacts 区域下载产物即可。

---

## 0. 前置准备(一次性)

### 0.1 GitHub 账号
如果没有,先去 https://github.com/signup 注册一个。

### 0.2 在 GitHub 上创建一个空仓库
1. 登录 GitHub → 右上角 `+` → **New repository**
2. **Repository name**:`libcn_clone`(可改名)
3. **Visibility**:**Private**(建议先私有,验证完再决定是否公开)
4. ⚠️ **不要勾选** "Add a README file" / "Add .gitignore" / "Choose a license"
   (我们本地已经有 README,空仓库即可)
5. 点 **Create repository**

记下仓库 URL,形如:
```
https://github.com/<你的用户名>/libcn_clone.git
```

### 0.3 Git 凭据(推送时需要)
- 推荐用 **SSH**:`ssh-keygen` 生成密钥,公钥贴到 GitHub Settings → SSH and GPG keys
- 或用 **HTTPS + Personal Access Token (PAT)**:Settings → Developer settings → PAT (classic),勾选 `repo` 权限

---

## 1. 一键推送(本地)

### 方式 A:Git Bash / WSL / Linux / macOS
```bash
cd "E:/书生插件/clone"
bash push_to_github.sh
```
按提示输入仓库 URL 即可。

### 方式 B:PowerShell
```powershell
cd "E:\书生\clone"
.\push_to_github.ps1
```

### 方式 C:手动
```bash
cd "E:/书生插件/clone"
git init
git add .
git commit -m "init: libcn_clone 完整工程骨架"
git branch -M main
git remote add origin <你的仓库URL>
git push -u origin main
```

---

## 2. 等 Actions 跑完(5–10 分钟)

1. 打开仓库页面 → 顶部 **Actions** 标签
2. 看到 **build-libcn-clone** workflow 在跑(黄色圆点转圈)
3. 点击进入 → 可以看到每个 step 的实时日志
4. 等绿色 ✓

### 常见失败与对策
| 报错 | 原因 | 对策 |
|------|------|------|
| `apt-get install failed` | 网络问题 | 重跑 workflow(右上 Re-run) |
| `make: gcc: Command not found` | apt 安装失败 | 看 step 日志,通常是网络 |
| `undefined reference to OpenSSL` | libssl-dev 未装 | 已加在 workflow,失败重跑 |
| `Error: libcn_clone.so not found` | make 编译失败 | 看 make 输出 |

---

## 3. 下载产物

1. workflow 跑成功后,进入对应 run
2. 滚动到底部 **Artifacts** 区域
3. 下载 **`libcn_clone`** zip(里面是 `libcn_clone.so`)
4. 解压得到 `.so` 文件

---

## 4. 验证产物(可选)

在 Linux 32 位环境运行 smoke_test:
```bash
file libcn_clone.so
# 应输出: ELF 32-bit LSB shared object, Intel 80386, ...

./smoke_test.sh
```

---

## 5. 部署到游戏服

把以下三个文件一起扔到游戏 `gamed/` 目录:
```
libcn_clone.so     # 你刚下载的
libdlc.data        # 你已有的(运行时配置)
resource.data      # 你已有的(加密资源包)
```

启动命令(在 gs.conf 所在目录):
```bash
LD_PRELOAD=./libcn_clone.so ./gs gs.conf gmserver.conf gsalias.conf &
```

启动后看 gs 控制台,应该看到:
```
[libcn_clone:init] libcn_clone.so loading...
[libcn_clone:init] ready (ver=N)
```
(N 是 libdlc.data 头部的版本号,通常是 57)

---

## 6. 之后每次更新

```bash
cd "E:/书生插件/clone"
git add .
git commit -m "fix: 修 xxx"
git push
```
Actions 自动跑,刷新 Artifacts 下载新版。

---

## ⚠️ 不要提交的内容(已由 .gitignore 自动排除)

- `clone/gs`(385 MB 游戏服务端,GitHub 100MB 限制)
- `libdlc.data` / `resource.data` / `ForDataEdit.DATA`(运行时配置/加密包,通常私有)
- `*.exe` / `*.so` / `*.dll`(逆向目标,版权敏感)
- `*.zip` / `*.rar`(大文件压缩包)

确认 `.gitignore` 工作正常:
```bash
git status     # 看到的是源码,不是那些大文件
git ls-files | wc -l    # 应该只有几十个文件
```