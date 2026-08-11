@echo off
rem 32位最小化构建
rem -Os 优化体积，-s 去除符号表
rem 注意：不要用 -nostartfiles/-Wl,-e,_WinMain@16 直接以 WinMain 为入口。
rem   那样会跳过 CRT 初始化，双击启动时 hInstance 等参数不可靠，导致窗口打不开。
rem   已实测：Start-Process 正常但 Explorer 双击崩溃。用标准 CRT 入口最稳妥。
rem 不加 app.rc：DPI 感知已在代码内通过 SetProcessDpiAwarenessContext 动态启用

set GCC="D:\Program Files\TDM-GCC-64\bin\gcc.exe"

%GCC% -m32 -Os -s -mwindows src\main.c src\crypto.c -o encrypt32.exe -lcomctl32 -lgdi32 -luser32

if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
echo BUILD OK
for %%F in (encrypt32.exe) do echo size: %%~zF bytes
