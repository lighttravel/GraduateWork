# Graduate PCB ESP32-S3 Firmware

ESP-IDF hardware validation firmware migrated from `lighttravel/GraduateWork` `codex/esp32s3` branch and trimmed for the new PCB.

## Hardware

- MCU: ESP32-S3-WROOM-1-N16R8, 16 MB flash, 8 MB Octal PSRAM
- Audio codec: ES8311 over I2C + I2S
- 4G module: ML307R over UART
- Keys: two active-low GPIO inputs
- Round TFT: pins reserved, not enabled in this build

## Pin map

| Peripheral | Signal | ESP32-S3 GPIO |
|---|---:|---:|
| ML307R | module TX -> S3 RX | GPIO9 |
| ML307R | S3 TX -> module RX | GPIO10 |
| ES8311 | I2C SCL | GPIO4 |
| ES8311 | I2C SDA | GPIO5 |
| I2S | MCLK | GPIO6 |
| I2S | CODEC DIN / S3 DOUT (speaker data) | GPIO11 |
| I2S | LRCK | GPIO12 |
| I2S | CODEC DOUT / S3 DIN (mic data) | GPIO13 |
| I2S | BCLK | GPIO14 |
| KEY1 | active-low input | GPIO8 |
| KEY2 | active-low input | GPIO3 |

## Build

```bash
idf.py set-target esp32s3
idf.py build
```

## Demo behavior

- Boot initializes NVS, keys, ES8311 audio, and ML307R UART.
- Press KEY1 to play a short speaker tone, then capture microphone audio for two seconds and print byte counts.
- Press KEY2 to run the full ML307R diagnostic: module info, firmware, IMEI, SIM readiness, ICCID, function level, signal, EPS/GSM/GPRS registration, packet attach, operator, PDP context, PDP active state, and PDP address. The firmware also runs this diagnostic automatically 20 seconds after boot and prints a PASS/FAIL summary.

Cloud ASR/TTS/chat and display code are intentionally not included in this build. The original hardcoded Wi-Fi and API credentials were not migrated.

## Host parser test

`powershell
gcc -std=c11 -Wall -Wextra -I.\tests\host\stubs -I.\main\services\ml307r -o .\build_host\test_ml307r_parser.exe .\tests\host\test_ml307r_parser.c
.\build_host\test_ml307r_parser.exe
` 


