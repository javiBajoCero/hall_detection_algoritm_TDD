
# previously ran on the console
# sudo apt update -y
# sudo apt install python3-venv python3-pip -y

# python -m pip install pyserial

#/dev/ttyACM0
#/dev/ttyAMA0


import serial
serTester = serial.Serial('/dev/ttyACM0', 115200, bytesize=8, parity='N', stopbits=1, timeout=5, xonxoff=0, rtscts=0)
serTarget = serial.Serial('/dev/ttyACM1', 115200, bytesize=8, parity='N', stopbits=1, timeout=5, xonxoff=0, rtscts=0)

serTester.write(b'ABC/n/r')     # write a string
line = serTester.readlines()   # read a '\n' terminated line
print(line)


