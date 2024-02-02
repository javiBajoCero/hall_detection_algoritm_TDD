
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
import time

def send_emulation_message(ser):
    message = "emulation\n\r"
    ser.write(message.encode())
    print(f'Sent message: {message}')

def receive_messages(ser):
    while True:
        if ser.in_waiting > 0:
            received_data = ser.readline().decode().strip()
            print(f"Received: {received_data}")

def main():

    try:
        # Open the serial connection
        ser = serial.Serial('/dev/ttyACM1', 115200, bytesize=8, parity='N', stopbits=1, timeout=2, xonxoff=0, rtscts=0)
        print(f"Serial connection opened ")

        # Start a thread or a separate process to receive incoming messages
        import threading
        receive_thread = threading.Thread(target=receive_messages, args=(ser,))
        receive_thread.start()

        # Send "emulation" message
        send_emulation_message(ser)

        # Wait for the receive thread to finish (or handle it differently based on your needs)
        receive_thread.join()

        # Close the serial connection
        ser.close()
        print("Serial connection closed.")

    except serial.SerialException as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()



