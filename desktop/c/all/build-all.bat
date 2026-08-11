@echo off
rem All-in-one (text + image) encrypt tool build
rem PNG native decode + JPG/BMP/GIF/TIFF via GDI+ (Win XP+, gdiplus.lib)
rem deps: KERNEL32/USER32/GDI32/COMCTL32/GDI+ + math
set GCC="D:\Program Files\TDM-GCC-64\bin\gcc.exe"
set SRC=src
set RES=resources

windres --use-temp-file --target=pe-i386 -i "%RES%\app.rc" -o app.o
if errorlevel 1 goto fail

%GCC% -m32 -Os -s -mwindows -I"%SRC%" -I"%SRC%\img" -o encrypt.exe "%SRC%\main.c" "%SRC%\crypto.c" "%SRC%\img\crypto_img.c" "%SRC%\img\png.c" "%SRC%\img\pngz.c" "%SRC%\img\image_decode.c" app.o -lcomctl32 -lgdi32 -luser32 -lgdiplus -lm

if errorlevel 1 goto fail
echo BUILD OK
for %%F in (encrypt.exe) do echo size: %%~zF bytes
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
