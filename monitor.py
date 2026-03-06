#!/usr/bin/env python3
import serial
import time

# 打开串口
ser = serial.Serial('COM9', 115200, timeout=1)

# 读取150行
for i in range(150):
    try:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
                print(line)
    except:
        break

ser.close()
