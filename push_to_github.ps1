# ============================================================
# push_to_github.ps1 - 一键把 libcn_clone 推到 GitHub (PowerShell)
# 用法: .\push_to_github.ps1
#       .\push_to_github.ps1 -Url https://github.com/user/repo.git
# ============================================================
[CmdletBinding()]
param(
    [string]$Url = ""
)

$ErrorActionPreference = "Stop"
$ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ROOT

function Write-Color($msg, $color = "White") {
    Write-Host $msg -ForegroundColor $color
}

Write-Color "=== libcn_clone 一键推送到 GitHub ===" "Green"
Write-Host "工程目录: $ROOT"
Write-Host ""

# 1) 检查 git
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Color "!!! git 未安装,请先装 Git for Windows" "Red"
    exit 1
}

# 2) URL
if (-not $Url) {
    Write-Color "请输入 GitHub 仓库 URL" "Yellow"
    Write-Host "格式: https://github.com/<用户名>/<仓库名>.git"
    Write-Host "   或: git@github.com:<用户名>/<仓库名>.git"
    $Url = Read-Host "URL"
    if (-not $Url) {
        Write-Color "!!! 未提供 URL" "Red"
        exit 1
    }
}

# 3) git init
if (-not (Test-Path ".git")) {
    Write-Color "[1/5] git init" "Cyan"
    git init -b main | Out-Null
} else {
    Write-Color "[1/5] 已初始化,跳过" "Cyan"
}

# 4) add
Write-Color "[2/5] git add ." "Cyan"
git add .

Write-Host ""
Write-Host "--- 待提交文件 ---"
git status --short
Write-Host "------------------"
Write-Host ""

# 5) 大文件检查
$staged = git diff --cached --name-only
$largeFiles = @()
foreach ($f in $staged) {
    if (Test-Path $f) {
        $size = (Get-Item $f).Length
        if ($size -gt 50MB) {
            $largeFiles += "$f ($([math]::Round($size/1MB,2)) MB)"
        }
    }
}
if ($largeFiles.Count -gt 0) {
    Write-Color "!!! 检测到大于 50MB 的文件:" "Red"
    $largeFiles | ForEach-Object { Write-Host "  $_" }
    Write-Host "请确认 .gitignore 是否覆盖,或者用 git rm --cached 排除"
    exit 1
}

# 6) commit
Write-Color "[3/5] git commit" "Cyan"
git commit -m "init: libcn_clone 完整工程骨架

- src/: main + hooks + config + resource + keycheck(5 模块)
- docs/: ARCHITECTURE / DATA_FORMAT / REVERSE_NOTES / GITHUB_SETUP
- Makefile + Dockerfile(32-bit ELF 编译)
- build.bat / build.sh / smoke_test.sh(跨平台一键)
- .github/workflows/build.yml(云端 Actions 编译)" 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Color "(可能已有 commit,继续)" "Yellow"
}

# 7) remote
Write-Color "[4/5] git remote" "Cyan"
$existing = git remote get-url origin 2>$null
if ($existing) {
    Write-Host "  origin 已存在: $existing"
    git remote set-url origin $Url
} else {
    git remote add origin $Url
}

# 8) push
Write-Color "[5/5] git push -u origin main" "Cyan"
Write-Host ""
Write-Color "即将推送到: $Url" "Yellow"
Write-Host "如果是 HTTPS,会要求输入 GitHub 用户名 + PAT(密码)"
Write-Host "如果是 SSH,需确保 ssh-agent 已加载密钥"
Write-Host ""
Read-Host "按回车开始推送,Ctrl+C 取消"
git push -u origin main

Write-Host ""
Write-Color "=== 推送完成 ===" "Green"
Write-Host ""
Write-Host "下一步:"
Write-Host "  1. 打开仓库页面 → Actions 标签"
Write-Host "  2. 等 build-libcn-clone workflow 跑完(约 5 分钟)"
Write-Host "  3. 在 run 详情底部 Artifacts 区域下载 libcn_clone.so"
Write-Host "  4. 部署到 gamed/ 目录,用 LD_PRELOAD 启动 gs"