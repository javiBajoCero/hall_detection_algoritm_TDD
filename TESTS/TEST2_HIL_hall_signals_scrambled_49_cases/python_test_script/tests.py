import serial
import serial.tools.list_ports
import time
import threading
import sys
from datetime import datetime

waittime=3;
serial_emulator=0;
serial_target=0;
succesfulltests=0;

def find_out_tester_and_open_ports():
    global serial_emulator;
    global serial_target;
        # Define the serial connection parameters
    baud_rate = 115200
    byte_size = 8
    parity = 'N'
    stop_bits = 1
    timeout = waittime
    xonxoff = 0
    rtscts = 0
    
    ports = serial.tools.list_ports.comports()
    print("Available serial ports:")
    for port in ports:
        if(port.pid==14155):
            print(f"- found target board in : {port.device}  with pid: {port.pid}")
            try:
                serial_target = serial.Serial(port=port.device,baudrate=baud_rate,bytesize=byte_size,parity=parity,stopbits=stop_bits,timeout=timeout,xonxoff=xonxoff,rtscts=rtscts)
                serial_target.flush();
                serial_target.isOpen()
                print ("port opened")
            except IOError:
                serial_target.close()
                serial_target.open()
                print ("port was already opened, i closed and opened again")
                
        elif (port.pid==14164):
            print(f"- found tester board in : {port.device}  with pid: {port.pid}")
            try:
                serial_emulator = serial.Serial(port=port.device,baudrate=baud_rate,bytesize=byte_size,parity=parity,stopbits=stop_bits,timeout=timeout,xonxoff=xonxoff,rtscts=rtscts)
                serial_emulator.flush();
                serial_emulator.isOpen()
                print ("port opened")
            except IOError:
                serial_emulator.close()
                serial_emulator.open()
                print ("port was already opened, i closed and opened again")


def setup_emulator(serial_out):
    print(f"setup_emulator")
    emulationmessage="emulation\n\r";
    serial_out.write(emulationmessage.encode());
    time.sleep(waittime)  # Add a delay to allow the device to process the message
        
def run_single_test(test_number,serial_out,serial_in,messages):
    global succesfulltests;
    successfullTEST=False;
    messages=messages +"\n\r"
    serial_out.write(messages.encode());
    time.sleep(waittime*0.1)  # Add a delay to allow the device to process the message
    
    resetmessage="reset target\n\r";
    serial_out.write(resetmessage.encode());
    time.sleep(waittime*0.1)  # Add a delay to allow the device to process the message

    start_time = datetime.now()
    received_data = ""
    numberofretries=0;
    while (datetime.now() - start_time).seconds <= waittime *0.7:

        if(datetime.now() - start_time).seconds >= waittime *0.3:
            numberofretries=numberofretries+1;
            resetmessage="reset target\n\r";
            serial_out.write(resetmessage.encode());
            time.sleep(waittime*0.1)  # Add a delay to allow the device to process the message
            
        if serial_in.in_waiting:
            received_data += serial_in.read(serial_in.in_waiting).decode()
            if '\r' in received_data:
                successfullTEST=True
                break

    received_data = received_data.replace('\r', '').replace('\n', '').replace(' ', '')
    messages = messages.replace('\r', '').replace('\n', '').replace(' ', '')
    
    if successfullTEST :
        if messages==received_data:
            print(f"SUCCESS TEST:{test_number}, {messages} == {received_data}, retries: {numberofretries}")
            succesfulltests=succesfulltests+1;
            sucess=True
        else:
            print(f"ERROR TEST:{test_number}, {messages} != {received_data}")
            sucess=False
    else:
        print(f"TIMEOUT TEST:{test_number}, {messages}")
        sucess=False

    time.sleep(waittime*0.1)  
    serial_out.flush()    
    serial_in.flush()


messages = [
            "ABC",
            "!ABC",
            "!A!BC",
            "!A!B!C",
            "!AB!C",
            "AB!C",
            "A!B!C",
            "A!BC",
            "BCA",
            "!BCA",
            "!B!CA",
            "!B!C!A",
            "!BC!A",
            "BC!A",
            "B!C!A",
            "B!CA",
            "CAB",
            "!CAB",
            "!C!AB",
            "!C!A!B",
            "!CA!B",
            "CA!B",
            "C!A!B",
            "C!AB",
            "ACB",
            "!ACB",
            "!A!CB",
            "!A!C!B",
            "!AC!B",
            "AC!B",
            "A!C!B",
            "A!CB",
            "BAC",
            "!BAC",
            "!B!AC",
            "!B!A!C",
            "!BA!C",
            "BA!C",
            "B!A!C",
            "B!AC",
            "CBA",
            "!CBA",
            "!C!BA",
            "!C!B!A",
            "!CB!A",
            "CB!A",
            "C!B!A",
            "C!BA",
        ]


def main():
    find_out_tester_and_open_ports();
    setup_emulator(serial_emulator);
    n=0
    for m in messages:
        n=n+1;
        run_single_test(n,serial_emulator,serial_target,m);
        
    if succesfulltests== len(messages):
        print("its all good man")
        sys.exit(0);
    else:
        print(f"MEEEEEEK! {succesfulltests} succesfull tests out of {len(messages)}")
        sys.exit(-1);





main()
