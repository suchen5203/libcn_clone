@echo off
REM ============================================================
REM  push.bat - double-click to push libcn_clone to GitHub
REM  Pure ASCII, no Chinese chars (avoid GBK/UTF-8 encoding bug)
REM ============================================================
setlocal EnableExtensions

echo.
echo  ==========================================
echo   libcn_clone - push to GitHub
echo  ==========================================
echo.

REM --- locate git.exe ---
set "GIT="
if exist "C:\Users\Administrator\.workbuddy\binaries\PortableGit\versions\1.2.0\cmd\git.exe" (
    set "GIT=C:\Users\Administrator\.workbuddy\binaries\PortableGit\versions\1.2.0\cmd\git.exe"
)
if "%GIT%"=="" if exist "C:\Program Files\Git\cmd\git.exe" (
    set "GIT=C:\Program Files\Git\cmd\git.exe"
)
if "%GIT%"=="" if exist "C:\Program Files (x86)\Git\cmd\git.exe" (
    set "GIT=C:\Program Files (x86)\Git\cmd\git.exe"
)
if "%GIT%"=="" (
    echo  [ERROR] git.exe not found.
    echo  Please install Git for Windows: https://git-scm.com/download/win
    echo.
    pause
    exit /b 1
)

REM --- add mingw64 bin to PATH so git can find its DLLs ---
set "MINGW=C:\Users\Administrator\.workbuddy\binaries\PortableGit\versions\1.2.0\mingw64\bin"
if exist "%MINGW%" set "PATH=%MINGW%;%PATH%"

REM --- enter this script's own directory (no hardcoded path) ---
cd /d "%~dp0"
if errorlevel 1 (
    echo  [ERROR] cannot enter directory: %~dp0
    pause
    exit /b 1
)

echo  [1/3] git: %GIT%
echo  [2/3] dir: %CD%
echo  [3/3] pushing to https://github.com/suchen5203/libcn_clone.git
echo.
echo  ----------------------------------------------------------
echo   You will be asked for:
echo     Username for 'https://github.com':  type  suchen5203
echo     Password for 'https://github.com':  paste your PAT
echo.
echo   Note: Password shows NO characters while typing/pasting.
echo   Just paste and press Enter.
echo  ----------------------------------------------------------
echo.

"%GIT%" -c credential.helper= push -u origin main

echo.
if %ERRORLEVEL%==0 (
    echo  ==========================================
    echo   PUSH SUCCESS!
    echo  ==========================================
    echo.
    echo   Next:
    echo    1. Open https://github.com/suchen5203/libcn_clone
    echo    2. Go to Actions tab, wait for build-libcn-clone
    echo    3. Download libcn_clone.so from Artifacts
) else (
    echo  ==========================================
    echo   PUSH FAILED. Send the error above to me.
    echo  ==========================================
)

echo.
pause