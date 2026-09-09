#!/usr/bin/env bash
# ============================================================
# push_to_github.sh - 一键把 libcn_clone 推到 GitHub
# 用法: bash push_to_github.sh
#       bash push_to_github.sh https://github.com/user/repo.git
# ============================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# 颜色
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== libcn_clone 一键推送到 GitHub ===${NC}"
echo "工程目录: $ROOT"
echo

# 1) 检查 git
if ! command -v git >/dev/null 2>&1; then
    echo -e "${RED}!!! git 未安装${NC}"
    exit 1
fi

# 2) 如果没传 URL,问
REMOTE_URL="${1:-}"
if [ -z "$REMOTE_URL" ]; then
    echo -e "${YELLOW}请输入 GitHub 仓库 URL${NC}"
    echo "格式: https://github.com/<用户名>/<仓库名>.git"
    echo "   或: git@github.com:<用户名>/<仓库名>.git"
    read -p "URL: " REMOTE_URL
    if [ -z "$REMOTE_URL" ]; then
        echo -e "${RED}!!! 未提供 URL${NC}"
        exit 1
    fi
fi

# 3) 如果已经 init 过,跳过
if [ ! -d ".git" ]; then
    echo "[1/5] git init"
    git init -b main
else
    echo "[1/5] 已初始化,跳过"
fi

# 4) 配置 .gitignore 已在仓库中,直接 add
echo "[2/5] git add ."
git add .

echo
echo "--- 待提交文件 ---"
git status --short
echo "------------------"
echo

# 5) 检查是否会误传大文件
LARGE_FILES=$(git status --short | awk '{print $2}' | \
    while read f; do
        [ -f "$f" ] && du -k "$f" 2>/dev/null | awk -v file="$f" '$1 > 51200 {print file, "("$1" KB)"}'
    done)

if [ -n "$LARGE_FILES" ]; then
    echo -e "${RED}!!! 检测到大于 50MB 的文件:${NC}"
    echo "$LARGE_FILES"
    echo "请确认 .gitignore 是否覆盖,或者用 git rm --cached 排除"
    exit 1
fi

# 6) 提交
echo "[3/5] git commit"
git commit -m "init: libcn_clone 完整工程骨架

- src/: main + hooks + config + resource + keycheck(5 模块)
- docs/: ARCHITECTURE / DATA_FORMAT / REVERSE_NOTES / GITHUB_SETUP
- Makefile + Dockerfile(32-bit ELF 编译)
- build.bat / build.sh / smoke_test.sh(跨平台一键)
- .github/workflows/build.yml(云端 Actions 编译)"

# 7) 配 remote
echo "[4/5] git remote"
if git remote get-url origin >/dev/null 2>&1; then
    echo "  origin 已存在: $(git remote get-url origin)"
    git remote set-url origin "$REMOTE_URL"
else
    git remote add origin "$REMOTE_URL"
fi

# 8) push
echo "[5/5] git push -u origin main"
echo
echo -e "${YELLOW}即将推送到: $REMOTE_URL${NC}"
echo "如果是 HTTPS,会要求输入 GitHub 用户名 + PAT(密码)"
echo "如果是 SSH,需确保 ssh-agent 已加载密钥"
echo
read -p "按回车开始推送,Ctrl+C 取消..."
git push -u origin main

echo
echo -e "${GREEN}=== 推送完成 ===${NC}"
echo
echo "下一步:"
echo "  1. 打开仓库页面 → Actions 标签"
echo "  2. 等 build-libcn-clone workflow 跑完(约 5 分钟)"
echo "  3. 在 run 详情底部 Artifacts 区域下载 libcn_clone.so"
echo "  4. 部署到 gamed/ 目录,用 LD_PRELOAD 启动 gs"