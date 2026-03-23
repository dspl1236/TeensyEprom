@echo off
echo ============================================
echo  Audi90 Tuner — Windows Setup
echo ============================================
echo.
echo Checking Python...
python --version 2>NUL
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Python not found!
    echo Please install Python 3.9+ from https://python.org
    pause
    exit /b 1
)

echo.
echo Initializing HachiROM submodule...
git submodule update --init --recursive
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Could not initialize HachiROM submodule.
    echo Run: git submodule update --init --recursive
)

echo.
echo Installing dependencies...
pip install PyQt5 pyserial matplotlib pyinstaller --quiet
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Some packages may have failed to install.
)

echo.
echo ============================================
echo  Setup complete! Run: run.bat
echo ============================================
pause
