#!/usr/bin/env bash
# ============================================================
#  libcn_clone 一键构建脚本 (Git Bash / Linux / macOS)
#  自动选择: docker > podman > 本地工具链
#  用法: bash build.sh
# ============================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
echo
echo "=== libcn_clone 一键构建 ==="
echo "工程目录: $ROOT"
echo

# ---------- 1) docker ----------
if command -v docker >/dev/null 2>&1; then
    echo "[1/3] 检测到 docker"
    echo "[2/3] 构建镜像 ..."
    docker build -t libcn_clone "$ROOT"
    echo "[3/3] 编译产物 ..."
    docker run --rm -v "$ROOT:/build" libcn_clone make check
    echo
    echo "=== 构建成功 ==="
    echo "产物: $ROOT/libcn_clone.so"
    exit 0
fi

# ---------- 2) podman ----------
if command -v podman >/dev/null 2>&1; then
    echo "[1/3] 检测到 podman"
    echo "[2/3] 构建镜像 ..."
    podman build -t libcn_clone "$ROOT"
    echo "[3/3] 编译产物 ..."
    podman run --rm -v "$ROOT:/build":Z libcn_clone make check
    echo
    echo "=== 构建成功 ==="
    echo "产物: $ROOT/libcn_clone.so"
    exit 0
fi

# ---------- 3) 本地工具链 ----------
if command -v gcc >/dev/null 2>&1; then
    if gcc -m32 -E - </dev/null >/dev/null 2>&1; then
        echo "[1/3] 检测到本地 gcc -m32"
        cd "$ROOT"
        make clean
        make USE_OPENSSL=1
        make check
        echo
        echo "=== 构建成功 ==="
        echo "产物: $ROOT/libcn_clone.so"
        exit 0
    fi
fi

# ---------- 4) 兜底: 提示 ----------
cat <<EOF
=== 未检测到可用工具链,需要先装一个 ===

选项 A(推荐): 安装 Docker Desktop
  https://www.docker.com/products/docker-desktop/
  装好后重新运行 bash build.sh

选项 B(Linux): apt 安装 32 位工具链
  sudo apt update
  sudo apt install -y gcc-multilib libssl-dev make
  装好后重新运行 bash build.sh

选项 C: 上 GitHub 用 Actions 云端构建
  推上 GitHub 后 .github/workflows/build.yml 会自动跑
  完成后在 Actions 页下载 artifact

选项 D(macOS): 装 colima(轻量 docker 替代)
  brew install colima docker
  colima start
  装好后重新运行 bash build.sh
EOF
exit 1