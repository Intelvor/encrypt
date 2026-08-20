@echo off
rem Build all platforms. Usage: scripts\build-all.bat [c|linux|android|all]
set ROOT=%~dp0..
set MODE=%1
if "%MODE%"=="" set MODE=all

if "%MODE%"=="c" goto c
if "%MODE%"=="linux" goto linux
if "%MODE%"=="android" goto android

echo ===== C desktop (Windows) =====
pushd "%ROOT%\desktop\c\all"
call build-all.bat
popd

echo ===== Linux (WSL) =====
set WSLPATH=/mnt/c/%ROOT:C:\=%
set WSLPATH=%WSLPATH:\=/%
wsl -e make -C "%WSLPATH%/desktop/linux"

echo ===== Android =====
pushd "%ROOT%\android"
call build-android.bat
popd
exit /b 0

:c
pushd "%ROOT%\desktop\c\all"
call build-all.bat
popd
exit /b 0

:linux
rem Convert Windows path to WSL mount path and run make
set WSLPATH=/mnt/c/%ROOT:C:\=%
set WSLPATH=%WSLPATH:\=/%
wsl -e make -C "%WSLPATH%/desktop/linux"
exit /b 0

:android
pushd "%ROOT%\android"
call build-android.bat
popd
exit /b 0
