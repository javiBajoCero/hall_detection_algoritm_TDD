
# previously ran on the console
# sudo apt update -y
# sudo apt install python3-venv python3-pip -y

# python -m pip install pyserial

#/dev/ttyACM0
#/dev/ttyAMA0

#python -m serial.tools.list_ports

import serial
import time

serTarget = serial.Serial('/dev/ttyACM2', 115200, bytesize=8, parity='N', stopbits=1, timeout=2, xonxoff=0, rtscts=0);
serTester = serial.Serial('/dev/ttyACM1', 115200, bytesize=8, parity='N', stopbits=1, timeout=2, xonxoff=0, rtscts=0);
time.sleep(0.300);
print("The ports are opened");


serTester.write(b'emulation/n/r') ;    # write a string
print(serTester.readlines());


serTester.close();
serTarget.close();
time.sleep(0.300);
print("The ports are closed");



