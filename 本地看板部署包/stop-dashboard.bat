@echo off
chcp 65001 >nul
taskkill /F /IM mosquitto.exe >nul 2>nul
if errorlevel 1 (echo MQTT 代理未在运行。) else (echo MQTT 代理已停止。)
timeout /t 3
