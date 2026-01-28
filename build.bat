@echo off
REM Build ActionEngine

set BUILD_DIR=%~dp0build

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo Configuring...
cmake .. -G "Visual Studio 17 2022" -A x64

if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

echo Building...
cmake --build . --config Release --parallel

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo.
echo Build successful!
echo Executable: %BUILD_DIR%\Release\Game.exe
