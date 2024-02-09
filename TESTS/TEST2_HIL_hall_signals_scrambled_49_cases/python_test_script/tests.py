import serial
import serial.tools.list_ports
import time
import threading
import sys

waittime=0.25;
stopthreads=True;
serial_tester=0;
serial_target=0;
send_index=1;

receivedMessages=[]
messages = [
            "ABC\n\r",
            "!ABC\n\r",
            "!A!BC\n\r",
            "!A!B!C\n\r",
            "!AB!C\n\r",
            "AB!C\n\r",
            "A!B!C\n\r",
            "A!BC\n\r",
            "BCA\n\r",
            "!BCA\n\r",
            "!B!CA\n\r",
            "!B!C!A\n\r",
            "!BC!A\n\r",
            "BC!A\n\r",
            "B!C!A\n\r",
            "B!CA\n\r",
            "CAB\n\r",
            "!CAB\n\r",
            "!C!AB\n\r",
            "!C!A!B\n\r",
            "!CA!B\n\r",
            "CA!B\n\r",
            "C!A!B\n\r",
            "C!AB\n\r",
            "ACB\n\r",
            "!ACB\n\r",
            "!A!CB\n\r",
            "!A!C!B\n\r",
            "!AC!B\n\r",
            "AC!B\n\r",
            "A!C!B\n\r",
            "A!CB\n\r",
            "BAC\n\r",
            "!BAC\n\r",
            "!B!AC\n\r",
            "!B!A!C\n\r",
            "!BA!C\n\r",
            "BA!C\n\r",
            "B!A!C\n\r",
            "B!AC\n\r",
            "CBA\n\r",
            "!CBA\n\r",
            "!C!BA\n\r",
            "!C!B!A\n\r",
            "!CB!A\n\r",
            "CB!A\n\r",
            "C!B!A\n\r",
            "C!BA\n\r",
        ]


def find_out_tester_and_open_ports():
    global serial_tester;
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
            f.write(f"- found target board in : {port.device}  with pid: {port.pid}")
            serial_target = serial.Serial(port=port.device,baudrate=baud_rate,bytesize=byte_size,parity=parity,stopbits=stop_bits,timeout=timeout,xonxoff=xonxoff,rtscts=rtscts)
            serial_target.flush();
        elif (port.pid==14164):
            print(f"- found tester board in : {port.device}  with pid: {port.pid}")
            f.write(f"- found tester board in : {port.device}  with pid: {port.pid}")
            serial_tester = serial.Serial(port=port.device,baudrate=baud_rate,bytesize=byte_size,parity=parity,stopbits=stop_bits,timeout=timeout,xonxoff=xonxoff,rtscts=rtscts)
            serial_tester.flush();
            
def send_messages_tester(ser, messages):
    resetmessage="reset target\n\r";
    emulationmessage="emulation\n\r";
    serial_tester.write(emulationmessage.encode());
    time.sleep(waittime)  # Add a delay to allow the device to process the message
    global send_index;
    global f
    for message in messages:
        serial_tester.write(message.encode())
        if message.find('reset')!=0:
            print(f'test case {send_index} ,messsage : {message}')
            f.write(f'test case {send_index} ,messsage : {message}')
            send_index=send_index+1;
        time.sleep(waittime/2)  # Add a delay to allow the device to process the message
        serial_tester.write(resetmessage.encode());
        time.sleep(waittime)  # Add a delay to allow the device to process the message


def receive_messages_tester(ser,messages):
    global stopthreads
    global send_index
    while stopthreads==True:
        if ser.in_waiting:
            received_data = ser.readline().decode()
            if received_data.find('\r')!=0:
                if send_index>1:
                    print(f"received from tester: {received_data}")

def receive_messages_target(ser,messages):
    global stopthreads
    global send_index
    global f
    while stopthreads==True:
        if ser.in_waiting:
            received_data = ser.readline().decode().replace(' ','')
            if received_data.find('\r')!=0:
                if send_index>1:
                    print(f"Received from target: {received_data}")
                    f.write(f"Received from target: {received_data}");
                    receivedMessages.append(received_data);

def main():
    global messages;
    # List and open available serial ports
    find_out_tester_and_open_ports()

    time.sleep(1); #enough time for the nucleo boards to be ready
    serial_target.flush();#flush again for good measure, sometimes we get garbage stuff send by the target board 
    serial_tester.flush();


    # Start a thread or a separate process to receive incoming messages
    receive_thread = threading.Thread(target=receive_messages_tester, args=(serial_tester,messages),daemon=True)

    #receive_thread.start();
    
    receive_thread_test = threading.Thread(target=receive_messages_target, args=(serial_target,messages),daemon=True)

    receive_thread_test.start();
    
    send_thread = threading.Thread(target=send_messages_tester, args=(serial_tester,messages),daemon=True) 
    send_thread.start();

    # Wait for the receive thread to finish (or handle it differently based on your needs)
    send_thread.join()
    global stopthreads
    stopthreads=False;
    #receive_thread.join();
    receive_thread_test.join();
    # Close the serial connection
    serial_tester.close()
    print("Serial connection closed.")
    f.write("Serial connection closed.")

    for i in range(len(messages)):
        messages[i]=messages[i].replace('\r','');
    
    if receivedMessages == messages:
        print("all test cases passed");
        f.write("all test cases passed");
        sys.exit(0);
    else:
        print("not all test cases passed");
        f.write("not all test cases passed");
        for i in range(len(receivedMessages)):
            if messages[i].replace('\r','')!=receivedMessages[i]:
                print(f"error in test case number {i}, it was supposed to be:{messages[i]} instead of: {receivedMessages[i]}")
                f.write(f"error in test case number {i}, it was supposed to be:{messages[i]} instead of: {receivedMessages[i]}")
        sys.exit(-1);
     

f = open("tests_python_console_dump.txt", "a").close();
main()
f.close();
