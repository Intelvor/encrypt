@echo off
rem All-in-one (text + image) encrypt tool build
rem All image decode (PNG/JPG/BMP/GIF/TIFF) via GDI+ (Win XP+, gdiplus.lib);
rem PNG encode via bundled png.c. Builds x86 and x64 versions.
rem deps: KERNEL32/USER32/GDI32/COMCTL32/GDI+ + math
set GCC="D:\Program Files\TDM-GCC-64\bin\gcc.exe"
set SRC=src
set RES=resources
set OUT=..\..\..\build
rem windres 2.25 --use-temp-file has a bug on Windows (CreateProcess null);
rem work around by invoking gcc as the preprocessor explicitly.
set PREARGS=--preprocessor-arg -E --preprocessor-arg -xc --preprocessor-arg -DRC_INVOKED

if not exist "%OUT%" mkdir "%OUT%"

rem ===== 32-bit (x86) =====
echo === Building 32-bit (x86) ===
windres --preprocessor "gcc" %PREARGS% --target=pe-i386 -i "%RES%\app.rc" -o "%OUT%\all-app32.o" -I "%RES%"
if errorlevel 1 goto fail
%GCC% -m32 -Os -s -mwindows -I"%SRC%" -I"%SRC%\img" -o "%OUT%\encrypt-x86.exe" "%SRC%\main.c" "%SRC%\crypto.c" "%SRC%\img\crypto_img.c" "%SRC%\img\png.c" "%SRC%\img\image_decode.c" "%OUT%\all-app32.o" -lcomctl32 -lgdi32 -luser32 -lgdiplus -lm
if errorlevel 1 goto fail

rem ===== 64-bit (x64) =====
echo === Building 64-bit (x64) ===
windres --preprocessor "gcc" %PREARGS% --target=pe-x86-64 -i "%RES%\app.rc" -o "%OUT%\all-app64.o" -I "%RES%"
if errorlevel 1 goto fail
%GCC% -m64 -Os -s -mwindows -I"%SRC%" -I"%SRC%\img" -o "%OUT%\encrypt-x64.exe" "%SRC%\main.c" "%SRC%\crypto.c" "%SRC%\img\crypto_img.c" "%SRC%\img\png.c" "%SRC%\img\image_decode.c" "%OUT%\all-app64.o" -lcomctl32 -lgdi32 -luser32 -lgdiplus -lm
if errorlevel 1 goto fail

echo BUILD OK
for %%F in ("%OUT%\encrypt-x86.exe") do echo x86 size: %%~zF bytes
for %%F in ("%OUT%\encrypt-x64.exe") do echo x64 size: %%~zF bytes
exit /b 0

:fail
echo BUILD FAILED
exit /b 1