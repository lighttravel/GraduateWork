import sys
import time

try:
    import serial
except ImportError:
    print("Python serial library not found. Please install: pip install pyserial")
    sys.exit(1)

def monitor_serial(port='COM9', baudrate=115200, duration=30):
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        ser.reset_input_buffer()
        print(f"Monitoring {port} at {baudrate} baud for {duration} seconds...")
        print("=" * 70)

        start_time = time.time()
        while (time.time() - start_time) < duration:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        print(line)
                except:
                    pass
            else:
                time.sleep(0.01)

        ser.close()
        print("=" * 70)
        print("Monitoring completed.")
        return 0

    except serial.SerialException as e:
        print(f"Serial port error: {e}")
        print("\nPossible solutions:")
        print("1. Close other programs using COM9 (PlatformIO Monitor, etc.)")
        print("2. Check if COM9 is the correct port")
        print("3. Try replugging the ESP32 USB cable")
        return 1
    except Exception as e:
        print(f"Unexpected error: {e}")
        return 1

if __name__ == "__main__":
    monitor_serial()
