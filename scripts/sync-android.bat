@echo off
rem Sync web/index.html to android/app/assets/index.html
rem (web & Android share one page; platform logic is feature-detected in-page, so a plain copy is enough)
setlocal
set ROOT=%~dp0..
set SRC=%ROOT%\web\index.html
set DST=%ROOT%\android\app\assets\index.html

if not exist "%SRC%" echo ERROR: %SRC% not found & exit /b 1
copy /Y "%SRC%" "%DST%" >nul
echo Synced: %SRC% -^> %DST%

rem Auto-commit if requested
if "%1"=="--commit" (
    cd /d "%ROOT%"
    git add android/app/assets/index.html
    git diff --cached --quiet android/app/assets/index.html
    if errorlevel 1 (
        git commit -m "Sync android asset from web"
        git push
        echo Committed and pushed.
    ) else (
        echo No changes to commit.
    )
)
