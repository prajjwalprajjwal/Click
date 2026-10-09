@echo off
setlocal

set "UF2_FILE=%~dp0click-rp2350.uf2"
if not exist "%UF2_FILE%" set "UF2_FILE=%~dp0firmware.uf2"
if not exist "%UF2_FILE%" (
    echo UF2 image not found beside this script.
    echo Download click-rp2350.uf2 into the same folder and retry.
    pause
    exit /b 1
)

set "PICOTOOL=picotool"
where picotool >nul 2>nul
if errorlevel 1 (
    set "PICOTOOL=%USERPROFILE%\.platformio\packages\tool-picotool-rp2040-earlephilhower\picotool.exe"
    if not exist "%PICOTOOL%" (
        echo picotool was not found. Install picotool or PlatformIO's RP2040 tool package.
        pause
        exit /b 1
    )
)

echo Updating RP2350 over USB with picotool. The device will briefly reset and reboot.
"%PICOTOOL%" load -f -u -v -x "%UF2_FILE%"
if errorlevel 1 (
    echo Picotool update failed. Check that the running firmware has USB Picotool support enabled.
    pause
    exit /b 1
)

echo RP2350 update complete.
pause