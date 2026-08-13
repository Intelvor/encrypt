@echo off
rem Sync web/index.html to docs/ for GitHub Pages deployment
setlocal
set ROOT=%~dp0..
set SRC=%ROOT%\web\index.html
set DST=%ROOT%\docs\index.html

if not exist "%SRC%" echo ERROR: %SRC% not found & exit /b 1
if not exist "%ROOT%\docs" mkdir "%ROOT%\docs"
copy /Y "%SRC%" "%DST%" >nul
echo Synced: %SRC% -^> %DST%

rem Auto-commit if requested
if "%1"=="--commit" (
    cd /d "%ROOT%"
    git add docs/index.html
    git diff --cached --quiet docs/index.html
    if errorlevel 1 (
        git commit -m "Sync docs/ for GitHub Pages"
        git push
        echo Committed and pushed.
    ) else (
        echo No changes to commit.
    )
)