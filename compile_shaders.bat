@echo off
REM Compile all GLSL shaders to SPIR-V
REM Requires Vulkan SDK with glslc compiler

setlocal EnableDelayedExpansion

set SCRIPT_DIR=%~dp0
set SHADER_DIR=%SCRIPT_DIR%shaders
set OUTPUT_DIR=%SCRIPT_DIR%shaders\compiled

REM Use VULKAN_SDK environment variable or find in default locations
if defined VULKAN_SDK (
    set GLSLC=%VULKAN_SDK%\Bin\glslc.exe
) else (
    REM Try to find Vulkan SDK in default install location
    for /d %%d in (C:\VulkanSDK\*) do (
        if exist "%%d\Bin\glslc.exe" (
            set GLSLC=%%d\Bin\glslc.exe
        )
    )
)

REM Fallback to PATH
if not defined GLSLC (
    set GLSLC=glslc
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo Compiling shaders...
echo Using: %GLSLC%

for %%f in ("%SHADER_DIR%\*.vert") do (
    echo   %%~nxf
    "%GLSLC%" "%%f" -o "%OUTPUT_DIR%\%%~nf_vert.spv" --target-env=vulkan1.0
    if errorlevel 1 (
        echo   ERROR: Failed to compile %%~nxf
        exit /b 1
    )
)

for %%f in ("%SHADER_DIR%\*.frag") do (
    echo   %%~nxf
    "%GLSLC%" "%%f" -o "%OUTPUT_DIR%\%%~nf_frag.spv" --target-env=vulkan1.0
    if errorlevel 1 (
        echo   ERROR: Failed to compile %%~nxf
        exit /b 1
    )
)

for %%f in ("%SHADER_DIR%\*.comp") do (
    echo   %%~nxf
    "%GLSLC%" "%%f" -o "%OUTPUT_DIR%\%%~nf_comp.spv" --target-env=vulkan1.0
    if errorlevel 1 (
        echo   ERROR: Failed to compile %%~nxf
        exit /b 1
    )
)

echo Done!
