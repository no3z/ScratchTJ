import serial

# Set up UART
ser = serial.Serial('/dev/serial0', 115200, timeout=0)  # Non-blocking mode

buffer = ""  # Temporary buffer for incomplete lines

while True:
    try:
        # Read available bytes
        data = ser.read(ser.in_waiting).decode('utf-8')
        buffer += data

        # Process complete lines
        while '\n' in buffer:
            line, buffer = buffer.split('\n', 1)  # Split buffer into line and remaining data
            if line:
                try:
                    faderValue, encoderValue = map(int, line.split())
                    print(f"Fader: {faderValue}, Encoder: {encoderValue}")
                except ValueError:
                    print("Malformed line, skipping...")

    except KeyboardInterrupt:
        print("Exiting...")
        break
    except Exception as e:
        print(f"Error: {e}")

