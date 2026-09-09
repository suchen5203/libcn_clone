# ============================================================
# push_safe.ps1 - 本地安全推送脚本
# token 仅在 git 弹窗里输入,不会进文件/对话/日志
# 用法: 在 PowerShell 里跑 .\push_safe.ps1
# ============================================================
$ErrorActionPreference = "Stop"
$ROOT = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ROOT

Write-Host "=== libcn_clone 本地安全推送 ===" -ForegroundColor Cyan
Write-Host "工程目录: $ROOT"
Write-Host "目标:     https://github.com/suchen5203/libcn_clone.git"
Write-Host ""

# 1) 确认 git 状态
$status = git status --porcelain
if ($status) {
    Write-Host "!!! 工作区有未提交改动:" -ForegroundColor Red
    Write-Host $status
    exit 1
}
Write-Host "[1/4] working tree clean OK"

# 2) 确认分支是 main
$branch = git rev-parse --abbrev-ref HEAD
if ($branch -ne "main") {
    Write-Host "!!! 当前分支: $branch(应为 main)" -ForegroundColor Red
    exit 1
}
Write-Host "[2/4] 分支: main OK"

# 3) 确认 remote
$remote = git remote get-url origin
Write-Host "[3/4] remote: $remote"

# 4) 提示关键步骤
Write-Host ""
Write-Host "===========================================" -ForegroundColor Yellow
Write-Host "接下来两件事,你必须先做完:" -ForegroundColor Yellow
Write-Host "===========================================" -ForegroundColor Yellow
Write-Host ""
Write-Host "[A] 先去 GitHub 建空仓库(否则 push 会 404):"
Write-Host "    1. 打开 https://github.com/new" -ForegroundColor White
Write-Host "    2. Repository name: libcn_clone" -ForegroundColor White
Write-Host "    3. Visibility: Private" -ForegroundColor White
Write-Host "    4. 4 个复选框 全不勾 (Add README / .gitignore / license / gitignore template)" -ForegroundColor White
Write-Host "    5. 点 Create repository" -ForegroundColor White
Write-Host ""
Write-Host "[B] 重新生成新 PAT(两个旧 token 已在对话泄露,作废):"
Write-Host "    1. 先去 https://github.com/settings/tokens 撤销旧的" -ForegroundColor White
Write-Host "    2. https://github.com/settings/tokens/new 生成新 token" -ForegroundColor White
Write-Host "    3. Note: libcn_clone v3, Expiration: 7 days, 只勾 repo" -ForegroundColor White
Write-Host "    4. 复制新 token,先放记事本(不要贴到任何对话)" -ForegroundColor White
Write-Host ""
Read-Host "A 和 B 都做完了?按回车继续,Ctrl+C 取消"

# 5) push
Write-Host ""
Write-Host "[4/4] git push -u origin main" -ForegroundColor Cyan
Write-Host "接下来 git 会弹窗:"
Write-Host "  Username for 'https://github.com': suchen5203"
Write-Host "  Password for 'https://github.com': <你的新 PAT,粘贴后回车>"
Write-Host ""
Write-Host "(PAT 在弹窗里输入,不会进任何文件/日志/对话)" -ForegroundColor Green
Write-Host ""

git push -u origin main

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "=== 推送成功 ===" -ForegroundColor Green
    Write-Host ""
    Write-Host "接下来:" -ForegroundColor Yellow
    Write-Host "  1. 打开 https://github.com/suchen5203/libcn_clone 确认文件都在" -ForegroundColor White
    Write-Host "  2. Actions 标签 等 build-libcn-clone 跑完(约 5 分钟)" -ForegroundColor White
    Write-Host "  3. 在 run 详情底部 Artifacts 下载 libcn_clone.so" -ForegroundColor White
    Write-Host "  4. 去 https://github.com/settings/tokens 撤销刚才用的 PAT" -ForegroundColor White
    Write-Host ""
    Write-Host "  想配 SSH 长期免密 push? 详见 docs/GITHUB_SETUP.md" -ForegroundColor Gray
} else {
    Write-Host ""
    Write-Host "!!! push 失败" -ForegroundColor Red
    Write-Host "常见原因:" -ForegroundColor Yellow
    Write-Host "  - 远程仓库还没建(去 https://github.com/new 建一下)" -ForegroundColor White
    Write-Host "  - PAT 没勾 repo 权限" -ForegroundColor White
    Write-Host "  - PAT 复制错了(密码框不显示,容易粘错)" -ForegroundColor White
}