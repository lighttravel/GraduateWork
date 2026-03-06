@echo off
echo 姝ｅ湪鍏抽棴鍗犵敤COM11鐨勮繘绋?..
for /f "tokens=5" %%a in ('netstat -aon ^| findstr COM11 ^| findstr LISTENING') do (
    echo 鍏抽棴杩涚▼ PID: %%a
    taskkill /F /PID %%a 2>nul
)
timeout /t 2 /nobreak >nul
echo 寮€濮嬩笂浼犲浐浠?..
pio run -e esp32-s3-n16r8 --target upload --upload-port COM11
pause
