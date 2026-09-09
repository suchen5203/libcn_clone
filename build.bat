@echo off
REM ============================================================
REM  libcn_clone 一键构建脚本 (Windows)
REM  自动选择: Docker Desktop > WSL > 提示手动安装
REM  用法: 双击运行 或 cmd /c build.bat
REM ============================================================
setlocal ENABLEDELAYEDEXPANSION
chcp 65001 >nul

set "ROOT=%~dp0"
echo.
echo === libcn_clone 一键构建 ===
echo 工程目录: %ROOT%
echo.

REM ---------- 1) Docker Desktop 路线 ----------
where docker >nul 2>&1
if %ERRORLEVEL%==0 (
    echo [1/3] 检测到 Docker,优先使用
    docker version >nul 2>&1
    if errorlevel 1 (
        echo   ! Docker 已安装但 daemon 未启动,请先启动 Docker Desktop
        echo   ! 或关闭此窗口后手动启动 Docker Desktop 再重跑
        goto :EOF
    )
    echo [2/3] 构建镜像 ...
    docker build -t libcn_clone "%ROOT%"
    if errorlevel 1 goto :build_fail
    echo [3/3] 编译产物 ...
    docker run --rm -v "%ROOT%:/build" libcn_clone make check
    if errorlevel 1 goto :build_fail
    echo.
    echo === 构建成功 ===
    echo 产物: %ROOT%libcn_clone.so
    goto :EOF
)

REM ---------- 2) WSL 路线 ----------
where wsl >nul 2>&1
if %ERRORLEVEL%==0 (
    echo [1/3] 检测到 WSL,使用 Ubuntu 编译
    wsl -e bash -lc "cd '%ROOT%' && make clean && make && file libcn_clone.so" 2>nul
    if errorlevel 1 (
        echo   ! WSL 已装但 Ubuntu 未初始化或未装 gcc-multilib
        echo   ! 请在 WSL 内执行一次: sudo apt update && sudo apt install -y gcc-multilib libssl-dev make
        echo   ! 然后重新运行本脚本
        goto :EOF
    )
    echo.
    echo === 构建成功 ===
    echo 产物: %ROOT%libcn_clone.so
    goto :EOF
)

REM ---------- 3) 兜底: 提示手动安装 ----------
echo === 未检测到 Docker / WSL,需要先装一个 ===
echo.
echo 选项 A(推荐): 安装 Docker Desktop
echo   下载: https://www.docker.com/products/docker-desktop/
echo   装好后重新运行本脚本
echo.
echo 选项 B: 启用 WSL2 + Ubuntu
echo   PowerShell(管理员): wsl --install
echo   首次启动 Ubuntu 后执行:
echo     sudo apt update ^&^& sudo apt install -y gcc-multilib libssl-dev make
echo   装好后重新运行本脚本
echo.
echo 选项 C: 上 GitHub 用 Actions 云端构建
echo   项目推上 GitHub 后,.github/workflows/build.yml 会自动跑
echo   完成后在 Actions 页下载 artifact
goto :EOF

:build_fail
echo.
echo !!! 构建失败,上面有错误输出
exit /b 1