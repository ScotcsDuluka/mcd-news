@echo off
echo ==========================================
echo  MCD News Mod - Build Script v4
echo ==========================================
echo.

REM === Find Visual Studio ===
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio not found!
    echo Install from: https://visualstudio.microsoft.com/visual-cpp-build-tools/
    pause
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set VS_PATH=%%i
if not defined VS_PATH (
    echo ERROR: Could not find VS installation!
    pause
    exit /b 1
)

echo Found Visual Studio: %VS_PATH%
echo.

REM === Setup x64 build environment ===
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

REM === Step 1: Generate pragma forwarders ===
echo [Step 1/2] Generating winhttp_pragmas.h...
powershell -ExecutionPolicy Bypass -File "%~dp0gen_pragmas.ps1"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to generate pragmas!
    pause
    exit /b 1
)

if not exist "%~dp0winhttp_pragmas.h" (
    echo ERROR: winhttp_pragmas.h not generated!
    pause
    exit /b 1
)

echo.

REM === Step 2: Compile ===
echo [Step 2/2] Compiling winhttp.dll (x64)...
echo.

cl /nologo /O2 /LD /EHsc /W3 /Fe:winhttp.dll mcd_news_hook.cpp /link kernel32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ==========================================
    echo  SUCCESS! winhttp.dll created!
    echo ==========================================
    echo.
    echo Next: Run install.bat (as Administrator)
) else (
    echo.
    echo ERROR: Build failed!
)

REM Clean up build artifacts
del /q *.obj *.exp *.lib 2>nul

pause
