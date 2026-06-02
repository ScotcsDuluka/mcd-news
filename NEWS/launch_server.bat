@echo off
title MCD News Server
echo ==================================================
echo   MCD News Server Launcher
echo ==================================================
echo.
echo This script will:
echo   1. Start the HTTPS server (serves news.json)
echo   2. You can then open Minecraft Dungeons
echo.
echo Press Ctrl+C to stop the server when done.
echo ==================================================
echo.

:: Check for admin
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [WARNING] Not running as Administrator!
    echo          Port 443 and cert install may require Admin.
    echo          Right-click this file and select "Run as Administrator"
    echo.
)

:: Check Python
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python not found! Please install Python 3.8+
    echo         https://www.python.org/downloads/
    pause
    exit /b 1
)

:: Run server
python "%~dp0server.py"

pause
