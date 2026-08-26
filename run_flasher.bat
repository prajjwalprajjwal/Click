@echo off
title "Clicker Web Flasher Server"
cd /d "%~dp0"
python tools\flasher\serve_flasher.py
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Failed to start local server. Please ensure Python is installed.
    pause
)
