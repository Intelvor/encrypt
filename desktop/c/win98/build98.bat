@echo off
rem Windows 98 兼容构建
rem Win98 自带 msvcrt.dll(6.0) 和 comctl32(4.72)，可直接用标准 CRT + W API
rem   -march=pentium   Pentium 无 CMOV/SSE，避免非法指令
rem   -Os -s           体积优化 + 去符号
rem 依赖：KERNEL32/USER32/GDI32/COMCTL32/msvcrt（Win98 全部自带）

set GCC="D:\Program Files\TDM-GCC-64\bin\gcc.exe"

%GCC% -m32 -march=pentium -Os -s -mwindows src\main.c src\crypto.c -o encrypt98.exe -lcomctl32 -lgdi32 -luser32

if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
echo BUILD OK
for %%F in (encrypt98.exe) do echo size: %%~zF bytes
