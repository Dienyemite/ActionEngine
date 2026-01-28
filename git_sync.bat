@echo off
:: Quick git commit and push script
:: Usage: git_sync.bat "Your commit message"

cd /d "%~dp0"

if "%~1"=="" (
    set MSG=Auto-update: %date% %time%
) else (
    set MSG=%~1
)

git add -A
git commit -m "%MSG%"
git push

echo.
echo Synced to GitHub!
