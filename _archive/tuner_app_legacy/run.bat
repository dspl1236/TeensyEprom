@echo off
cd /d "%~dp0"
python main.py
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: App crashed. Check dependencies with install.bat
    pause
)
