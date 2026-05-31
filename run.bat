@echo off
setlocal
set "QT_BIN=D:\Qt\6.11.1\mingw_64\bin"
set "MINGW_BIN=D:\Qt\Tools\mingw1310_64\bin"

if not exist "%QT_BIN%\Qt6Core.dll" (
    echo [错误] 未找到 Qt：%QT_BIN%
    echo 请检查 Qt 安装路径，或修改本脚本中的 QT_BIN / MINGW_BIN。
    pause
    exit /b 1
)

set "PATH=%QT_BIN%;%MINGW_BIN%;%PATH%"
cd /d "%~dp0"
start "" "%~dp0build-qt-ninja\Chrona.exe"
