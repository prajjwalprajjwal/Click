@echo off
setlocal enabledelayedexpansion
title "Clicker ESP32 CLI Flasher"

cd /d "%~dp0"

:: Check if Python is available
python --version >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Python is not installed or not found in PATH!
    echo Please install Python or use the web flasher via run_flasher.bat
    pause
    exit /b 1
)

:: Run the interactive desktop flasher
python tools\flasher\flash_device.py %*
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Flashing failed or was cancelled.
    pause
    exit /b %ERRORLEVEL%
)
