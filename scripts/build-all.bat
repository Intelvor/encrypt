@echo off
rem Build all platforms. Usage: scripts\build-all.bat [c|android|all]
set ROOT=%~dp0..
set MODE=%1
if "%MODE%"=="" set MODE=all

if "%MODE%"=="c" goto c
if "%MODE%"=="android" goto android

echo ===== C desktop =====
pushd "%ROOT%\desktop\c\all"
call build-all.bat
popd

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

:android
pushd "%ROOT%\android"
call build-android.bat
popd
exit /b 0
