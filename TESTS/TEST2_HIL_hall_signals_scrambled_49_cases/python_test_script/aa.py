import serial
import serial.tools.list_ports


ports = serial.tools.list_ports.comports()

print(ports)
