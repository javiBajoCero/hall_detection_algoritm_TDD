
# previously ran on the console
# sudo apt update -y
# sudo apt install python3-venv python3-pip -y

# python -m pip install pyserial

#/dev/ttyACM0
#/dev/ttyAMA0

#python -m serial.tools.list_ports

#import serial
#import time
#
#serTarget = serial.Serial('/dev/ttyACM2', 115200, bytesize=8, parity='N', stopbits=1, timeout=2, xonxoff=0, rtscts=0);
#serTester = serial.Serial('/dev/ttyACM1', 115200, bytesize=8, parity='N', stopbits=1, timeout=2, xonxoff=0, rtscts=0);
#time.sleep(0.300);
#print("The ports are opened");
#
#
#serTester.write(b'emulation/n/r') ;    # write a string
#print(serTester.read(size=50));
#
#
#serTester.close();
#serTarget.close();
#time.sleep(0.300);
#print("The ports are closed");
#

import serial
import serial.tools.list_ports
import time

def list_available_ports():
    ports = serial.tools.list_ports.comports()
    print("Available serial ports:")
    for port in ports:
        print(f"- {port.device}")

def send_messages(ser, messages):
    for message in messages:
        ser.write(message.encode())
        print(f'Sent message: {message}')
        time.sleep(1)  # Add a delay to allow the device to process the message

def receive_messages(ser, duration):
    start_time = time.time()
    while time.time() - start_time < duration:
        if ser.in_waiting > 0:
            received_data = ser.readline().decode().strip()
            print(f"Received: {received_data}")

def main():
    # List available serial ports
    list_available_ports()

    # Define the serial connection parameters
    baud_rate = 115200
    byte_size = 8
    parity = 'N'
    stop_bits = 1
    timeout = 10  # Increased timeout
    xonxoff = 0
    rtscts = 0

    try:
        ser = None
        for port_info in serial.tools.list_ports.comports():
            try:
                # Try opening the serial connection on each available port
                ser = serial.Serial(
                    port=port_info.device,
                    baudrate=baud_rate,
                    bytesize=byte_size,
                    parity=parity,
                    stopbits=stop_bits,
                    timeout=timeout,
                    xonxoff=xonxoff,
                    rtscts=rtscts
                )
                print(f"Serial connection opened on {port_info.device} at {baud_rate} baud.")
                break  # Stop iterating if a valid connection is established

            except serial.SerialException as e:
                print(f"Error opening port {port_info.device}: {e}")

        if ser is not None:
            # Define the messages to be sent
            messages = [
              "emulation\n\r",
                "ABC\n\r",
                "reset target\n\r",
                "!ABC\n\r",
                "reset target\n\r",
                "!A!BC\n\r",
                "reset target\n\r",
                "!A!B!C\n\r",
                "reset target\n\r",
                "!AB!C\n\r",
                "reset target\n\r",
                "AB!C\n\r",
                "reset target\n\r",
                "A!B!C\n\r",
                "reset target\n\r",
                "A!BC\n\r",
                "reset target\n\r",
                "BCA\n\r",
                "reset target\n\r",
                "!BCA\n\r",
                "reset target\n\r",
                "!B!CA\n\r",
                "reset target\n\r",
                "!B!C!A\n\r",
                "reset target\n\r",
                "!BC!A\n\r",
                "reset target\n\r",
                "BC!A\n\r",
                "reset target\n\r",
                "B!C!A\n\r",
                "reset target\n\r",
                "B!CA\n\r",
                "reset target\n\r",
                "CAB\n\r",
                "reset target\n\r",
                "!CAB\n\r",
                "reset target\n\r",
                "!C!AB\n\r",
                "reset target\n\r",
                "!C!A!B\n\r",
                "reset target\n\r",
                "!CA!B\n\r",
                "reset target\n\r",
                "CA!B\n\r",
                "reset target\n\r",
                "C!A!B\n\r",
                "reset target\n\r",
                "C!AB\n\r",
                "reset target\n\r",
                "ACB\n\r",
                "reset target\n\r",
                "!ACB\n\r",
                "reset target\n\r",
                "!A!CB\n\r",
                "reset target\n\r",
                "!A!C!B\n\r",
                "reset target\n\r",
                "!AC!B\n\r",
                "reset target\n\r",
                "AC!B\n\r",
                "reset target\n\r",
                "A!C!B\n\r",
                "reset target\n\r",
                "A!CB\n\r",
                "reset target\n\r",
                "BAC\n\r",
                "reset target\n\r",
                "!BAC\n\r",
                "reset target\n\r",
                "!B!AC\n\r",
                "reset target\n\r",
                "!B!A!C\n\r",
                "reset target\n\r",
                "!BA!C\n\r",
                "reset target\n\r",
                "BA!C\n\r",
                "reset target\n\r",
                "B!A!C\n\r",
                "reset target\n\r",
                "B!AC\n\r",
                "reset target\n\r",
                "CBA\n\r",
                "reset target\n\r",
                "!CBA\n\r",
                "reset target\n\r",
                "!C!BA\n\r",
                "reset target\n\r",
                "!C!B!A\n\r",
                "reset target\n\r",
                "!CB!A\n\r",
                "reset target\n\r",
                "CB!A\n\r",
                "reset target\n\r",
                "C!B!A\n\r",
                "reset target\n\r",
                "C!BA\n\r",
                "reset target\n\r",
            ]

            # Start a thread or a separate process to receive incoming messages
            import threading
            receive_thread = threading.Thread(target=receive_messages, args=(ser, 10))  # Run for 10 seconds
            receive_thread.start()

            # Send messages
            send_messages(ser, messages)

            # Wait for the receive thread to finish (or handle it differently based on your needs)
            receive_thread.join()

            # Close the serial connection
            ser.close()
            print("Serial connection closed.")

    except serial.SerialException as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()





