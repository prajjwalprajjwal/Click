@echo off
setlocal enabledelayedexpansion
title "Clicker ESP32 Flasher (Windows)"

cd /d "%~dp0"

echo ===================================================
echo             Clicker ESP32 Firmware Flasher
echo ===================================================
echo.

python --version >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Python is not installed or not found in PATH!
    echo Please install Python 3 and esptool: pip install esptool
    pause
    exit /b 1
)

:: Run the flash utility
if exist "..\..\tools\flasher\flash_device.py" (
    python ..\..\tools\flasher\flash_device.py %*
) else (
    echo Flashing firmware via esptool...
    esptool.py --chip esp32 write_flash 0x1000 ..\..\.pio\build\esp32doit-devkit-v1\bootloader.bin 0x8000 ..\..\.pio\build\esp32doit-devkit-v1\partitions.bin 0x10000 ..\..\.pio\build\esp32doit-devkit-v1\firmware.bin
)

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Flashing failed.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [SUCCESS] Flashing complete!
pause
