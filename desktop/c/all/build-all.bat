@echo off
rem All-in-one (text + image) encrypt tool build
rem All image decode (PNG/JPG/BMP/GIF/TIFF) via GDI+ (Win XP+, gdiplus.lib);
rem PNG encode via bundled png.c. 32-bit build only (x64 has no perf advantage for this workload).
rem deps: KERNEL32/USER32/GDI32/COMCTL32/GDI+ + math
set GCC="D:\Program Files\TDM-GCC-64\bin\gcc.exe"
set SRC=src
set RES=resources
set OUT=..\..\..\build
rem windres 2.25 --use-temp-file has a bug on Windows (CreateProcess null);
rem work around by invoking gcc as the preprocessor explicitly.
set PREARGS=--preprocessor-arg -E --preprocessor-arg -xc --preprocessor-arg -DRC_INVOKED

if not exist "%OUT%" mkdir "%OUT%"

echo === Building 32-bit (x86) ===
windres --preprocessor "gcc" %PREARGS% --target=pe-i386 -i "%RES%\app.rc" -o "%OUT%\all-app.o" -I "%RES%"
if errorlevel 1 goto fail
%GCC% -m32 -Os -s -mwindows -I"%SRC%" -I"%SRC%\img" -o "%OUT%\encrypt.exe" "%SRC%\main.c" "%SRC%\crypto.c" "%SRC%\img\crypto_img.c" "%SRC%\img\png.c" "%SRC%\img\image_decode.c" "%OUT%\all-app.o" -lcomctl32 -lgdi32 -luser32 -lgdiplus -lole32 -lm
if errorlevel 1 goto fail

echo BUILD OK
for %%F in ("%OUT%\encrypt.exe") do echo size: %%~zF bytes
exit /b 0

:fail
echo BUILD FAILED
exit /b 1