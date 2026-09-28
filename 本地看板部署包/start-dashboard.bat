@echo off
chcp 65001 >nul
title 智能门锁看板启动器
cd /d "%~dp0"

REM ---- 找到 mosquitto.exe（优先 PATH，其次默认安装目录）----
set "MOSQ=mosquitto.exe"
where mosquitto.exe >nul 2>nul
if errorlevel 1 (
    if exist "C:\Program Files\mosquitto\mosquitto.exe" (
        set "MOSQ=C:\Program Files\mosquitto\mosquitto.exe"
    ) else (
        echo [错误] 没找到 mosquitto.exe
        echo 请先双击本目录里的 mosquitto-2.1.2-install-windows-x64.exe 安装，
        echo 或去 https://mosquitto.org/download/ 下载 Windows 64 位版。
        pause
        exit /b 1
    )
)

REM ---- 若代理已在运行则跳过启动 ----
tasklist /FI "IMAGENAME eq mosquitto.exe" 2>nul | find /I "mosquitto.exe" >nul
if errorlevel 1 (
    echo 正在启动 MQTT 代理 (1883 / 9001) ...
    start "MQTT代理" /min "%MOSQ%" -c "%~dp0mosquitto.conf" -v
    timeout /t 2 /nobreak >nul
) else (
    echo MQTT 代理已在运行，跳过启动。
)

REM ---- 启动看板 HTTP 服务(手机入口) ----
powershell -NoProfile -Command "try { (New-Object System.Net.WebClient).DownloadString('http://192.168.137.1:8000/') > $null; exit 0 } catch { exit 1 }" 2>nul
if errorlevel 1 (
    echo 正在启动看板网页服务 (手机入口) ...
    start "看板网页服务" /min powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0serve-dashboard.ps1"
    timeout /t 2 /nobreak >nul
)

REM ---- 显示本机 IP，方便填固件 ----
echo.
echo ============================================
echo   本机 IPv4 地址（门锁固件 BEMFA_HOST 填这个）：
ipconfig | findstr /R "IPv4"
echo.
echo   手机看板入口（手机连本机热点后浏览器打开）：
echo   http://192.168.137.1:8000
echo ============================================
echo.

REM ---- 打开看板 ----
start "" "%~dp0index.html"

echo 看板已打开。关闭本窗口不会停止代理（代理在后台运行）。
echo 如需停止代理：任务管理器结束 mosquitto.exe 或运行 stop-dashboard.bat
timeout /t 8
exit /b 0
