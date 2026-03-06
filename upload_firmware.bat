@echo off
echo ========================================
echo ESP32-S3-N16R8 Firmware Upload Tool
echo ========================================
echo.
echo This will upload the I2C diagnostic firmware:
echo   I2C_SCL = GPIO4
echo   I2C_SDA = GPIO5
echo.
echo Please:
echo 1. Close all serial monitors (Arduino IDE, VSCode, etc.)
echo 2. Press ENTER to continue
echo.
pause

echo.
echo Waiting for port...
timeout /t 3 /nobreak >nul

echo.
echo Uploading firmware...
pio run -e esp32-s3-n16r8 --target upload --upload-port COM11

echo.
echo ========================================
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Firmware uploaded!
    echo.
    echo You can now open the serial monitor to test.
) else (
    echo [FAILED] Upload failed!
    echo.
    echo Please check:
    echo - COM11 port is not in use
    echo - ESP32-S3 is properly connected
    echo - Drivers are installed
)
echo ========================================
echo.
pause
