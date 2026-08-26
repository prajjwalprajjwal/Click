@echo off
setlocal enabledelayedexpansion
title "Clicker ESP32 Release Manager"

cd /d "%~dp0"

:: Check if Python is available
python --version >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Python is not installed or not found in PATH!
    echo Please install Python to use the interactive release manager.
    pause
    exit /b 1
)

:: Run the interactive release manager
python tools\release\release_manager.py %*
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Release process exited with errors.
    pause
    exit /b %ERRORLEVEL%
)
