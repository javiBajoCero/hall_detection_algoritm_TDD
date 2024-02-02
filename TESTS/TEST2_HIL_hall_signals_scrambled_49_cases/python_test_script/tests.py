
# previously ran on the console
# sudo apt update -y
# sudo apt install python3-venv python3-pip -y

# python -m pip install pyserial

#/dev/ttyACM0
#/dev/ttyAMA0


import serial
import time


serTester = serial.Serial('/dev/ttyACM0', 115200, bytesize=8, parity='N', stopbits=1, timeout=1, xonxoff=0, rtscts=0)
serTarget = serial.Serial('/dev/ttyACM1', 115200, bytesize=8, parity='N', stopbits=1, timeout=1, xonxoff=0, rtscts=0)
time.sleep(0.300)
print("The ports are opened")

serTester.write(b'emulation/n/r')     # write a string
line = serTester.readline()
print(line)
line = serTester.readline()
print(line)

serTarget.write(b'emulation/n/r')     # write a string
line = serTarget.readline()
print(line)
line = serTarget.readline()
print(line)

serTester.close()
serTarget.close()
time.sleep(0.300)
print("The ports are closed")



