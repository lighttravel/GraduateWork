@echo off
setlocal enabledelayedexpansion
set "PORT=COM9"
set "BAUD=115200"

powershell -NoProfile -ExecutionPolicy Bypass -Command "& \$port = new-Object System.IO.Ports.SerialPort('%PORT%', %BAUD%, 'None', 8, 'None'); \$port.Open(); \$port.DtrEnable = \$true; \$port.ReadTimeout = 5000; while(\$true) { \$line = \$port.ReadLine(); if(-not \$line) { break } Write-Host \$line }; \$port.Close()"

