@echo off
setlocal enabledelayedexpansion

REM ============================================
REM  Configure your game path here:
REM ============================================
set "GAME_BIN=D:\SteamLibrary\Steamapps\Common\Minecraft Dungeons\Dungeons\Binaries\Win64"

echo ==========================================
echo  MCD News Mod - Uninstaller v4
echo ==========================================
echo.

REM === Step 1: Remove hosts entry ===
echo [Step 1/3] Removing hosts entry...
findstr /V /C:"launchercontent.mojang.com" C:\Windows\System32\drivers\etc\hosts > C:\Windows\Temp\hosts_tmp
if !ERRORLEVEL! EQU 0 (
    copy /Y C:\Windows\Temp\hosts_tmp C:\Windows\System32\drivers\etc\hosts >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        echo   OK: Removed hosts entry
    ) else (
        echo   WARNING: Could not update hosts file (need admin?)
    )
) else (
    echo   OK: Hosts entry not found (already clean)
)
del /q C:\Windows\Temp\hosts_tmp 2>nul

REM === Step 2: Remove proxy DLL ===
echo.
echo [Step 2/3] Removing proxy winhttp.dll...
del /f "%GAME_BIN%\winhttp.dll" 2>nul
if exist "%GAME_BIN%\winhttp.dll" (
    echo   WARNING: Could not delete - try closing game first
) else (
    echo   OK: Removed winhttp.dll
)

REM === Step 3: Remove original backup ===
echo.
echo [Step 3/3] Removing winhttp_orig.dll...
del /f "%GAME_BIN%\winhttp_orig.dll" 2>nul
if exist "%GAME_BIN%\winhttp_orig.dll" (
    echo   WARNING: Could not delete winhttp_orig.dll
) else (
    echo   OK: Removed winhttp_orig.dll
)

echo.
echo ==========================================
echo  UNINSTALL COMPLETE!
echo ==========================================
echo.
echo  The game will now use the system winhttp.dll
echo  and fetch news from the original Mojang server.
echo.
pause
