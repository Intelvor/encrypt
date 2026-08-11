@echo off
rem 图片加解密工具构建（32位）
rem 依赖：KERNEL32/USER32/GDI32/COMCTL32/msvcrt + math
set GCC="D:\Program Files\TDM-GCC-64\bin\gcc.exe"

%GCC% -m32 -Os -s -mwindows -I. -o img-encrypt.exe src\main.c src\png.c src\pngz.c src\crypto_img.c src\image_decode.c -lcomctl32 -lgdi32 -luser32 -lgdiplus -lm

if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
echo BUILD OK
for %%F in (img-encrypt.exe) do echo size: %%~zF bytes
