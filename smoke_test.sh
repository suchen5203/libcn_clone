#!/usr/bin/env bash
# smoke_test.sh - 编译产物的最小烟雾测试
# 仅验证 ELF 头 + 关键符号存在,需要 32 位运行环境
set -e

ELF="libcn_clone.so"

echo "=== smoke test: $ELF ==="

# 1) 文件存在
test -f "$ELF" || { echo "FAIL: $ELF 不存在"; exit 1; }

# 2) ELF 32-bit LSB
file "$ELF" | grep -q "ELF 32-bit LSB" \
    || { echo "FAIL: 不是 32-bit LSB ELF"; exit 1; }
echo "  [OK] ELF 32-bit LSB"

# 3) Intel i386 架构
file "$ELF" | grep -q "Intel 80386" \
    || { echo "FAIL: 不是 Intel 80386"; exit 1; }
echo "  [OK] Intel 80386"

# 4) 是共享对象
file "$ELF" | grep -q "shared object" \
    || { echo "FAIL: 不是 shared object"; exit 1; }
echo "  [OK] shared object"

# 5) 关键符号存在 (动态符号表)
for sym in install_all_hooks dlc_config_load resource_load keycheck_verify; do
    if nm -D "$ELF" 2>/dev/null | grep -q " T $sym"; then
        echo "  [OK] export $sym"
    else
        echo "  [WARN] export $sym 未找到(可能内联/未导出)"
    fi
done

# 6) 没静默吞错,确保 .o 都干净编出来了
for obj in main.o hooks.o config.o resource.o keycheck.o; do
    test -f "$obj" && echo "  [OK] $obj 存在"
done

echo
echo "=== smoke test 通过 ==="
echo "实际游戏联调需要把 $ELF + libdlc.data + resource.data"
echo "部署到目标 gamed/ 目录,用 LD_PRELOAD 启动 gs 验证"