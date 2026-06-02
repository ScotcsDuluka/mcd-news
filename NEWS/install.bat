@echo off
setlocal enabledelayedexpansion

REM ============================================
REM  Configure your game path here:
REM ============================================
set "GAME_BIN=D:\SteamLibrary\Steamapps\Common\Minecraft Dungeons\Dungeons\Binaries\Win64"

echo ==========================================
echo  MCD News Mod - Installer v4
echo ==========================================
echo.

REM === Check if winhttp.dll exists in current directory ===
if not exist "%~dp0winhttp.dll" (
    echo ERROR: winhttp.dll not found!
    echo Run build.bat first to compile the DLL.
    pause
    exit /b 1
)

REM === Check if game directory exists ===
if not exist "%GAME_BIN%" (
    echo ERROR: Game directory not found!
    echo Expected: %GAME_BIN%
    echo.
    echo Edit install.bat and change GAME_BIN to your game path.
    pause
    exit /b 1
)

echo Game directory: %GAME_BIN%
echo.

REM === Step 1: Add hosts entry ===
echo [Step 1/3] Adding hosts entry...
findstr /C:"launchercontent.mojang.com" C:\Windows\System32\drivers\etc\hosts >nul 2>&1
if !ERRORLEVEL! NEQ 0 (
    echo 127.0.0.1 launchercontent.mojang.com>> C:\Windows\System32\drivers\etc\hosts
    if !ERRORLEVEL! EQU 0 (
        echo   OK: Added hosts entry
    ) else (
        echo   WARNING: Could not modify hosts file
        echo   Try running as Administrator.
    )
) else (
    echo   OK: Hosts entry already exists
)

REM === Step 2: Setup winhttp_orig.dll ===
echo.
echo [Step 2/3] Setting up winhttp_orig.dll...
if not exist "%GAME_BIN%\winhttp_orig.dll" (
    copy "C:\Windows\System32\winhttp.dll" "%GAME_BIN%\winhttp_orig.dll" >nul
    if !ERRORLEVEL! EQU 0 (
        echo   OK: Copied system winhttp.dll as winhttp_orig.dll
    ) else (
        echo   ERROR: Failed to copy system winhttp.dll
        pause
        exit /b 1
    )
) else (
    echo   OK: winhttp_orig.dll already exists
)

REM === Step 3: Install proxy DLL ===
echo.
echo [Step 3/3] Installing proxy winhttp.dll...
copy /Y "%~dp0winhttp.dll" "%GAME_BIN%\winhttp.dll" >nul
if !ERRORLEVEL! EQU 0 (
    echo   OK: Installed winhttp.dll
) else (
    echo   ERROR: Failed to install winhttp.dll
    pause
    exit /b 1
)

echo.
echo ==========================================
echo  INSTALL COMPLETE!
echo ==========================================
echo.
echo  When you open the game:
echo  - DLL will auto-start a local HTTPS server
echo  - Server fetches news from GitHub Pages
echo  - When game closes, server stops automatically
echo.
echo  To uninstall: Run uninstall.bat
echo ==========================================
echo.
pause
