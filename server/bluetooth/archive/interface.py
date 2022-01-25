import serial
import os
import time

ser = serial.Serial()

# Check if the device is connected and able to send/recieve commands
def is_connected():
    global ser

    try:
        ser.write(b'IS_OK\n')
        isok = ser.readline()
        if isok == b'OK\n':
            return True
        else:
            return False
    except:
        return False

# Repeatedly check for connection until a device is connected 
def wait_for_device_connection():
    global ser

    print("Waiting for device re-connection...")
    
    # Start listening for connection requests
    os.system("sudo rfcomm listen hci0 &")
    print("Connections are now being accepted")
    
    # Repeatedly try to open the serial port until device connection is established
    while True:
        try:
            if ser.isOpen():
                ser.close()
            ser = serial.Serial('/dev/rfcomm0')
            if is_connected():
                break
        except:
            time.sleep(1)

    print("Connected")


    
# -- Step 1. Initiate a connection

wait_for_device_connection()

# -- Step 2. Start listening to commands
print("Listening to commands...")
while True:
    try:
        cmd = ser.readline()
        
        if cmd == b'CMD_START\n':
            # Start recording
            print("Recording...")
            os.system("echo $RANDOM > rand.txt")

        elif cmd == b'CMD_STOP\n':
            # Stop recording
            print("Stopping...")

        elif cmd == b'CMD_TRANSFER\n':
            # Transfer the recorded file
            # The client is supposed have sent it's MAC address right after issuing a transfer command
            mac_addr = "FC:D4:36:16:AF:41" 
            try:
                channel_n = int(subprocess.run("./find_obex_channel.sh " + mac_addr, shell=True, capture_output=True, check=True).stdout)
            except:
                print("[ERR] Couldn't find channel for device")
            
            print("Transferring file to", mac_addr, "...)")
            
            try:
                subprocess.run("obexftp --nopath --noconn --uuid none --bluetooth " + mac_addr + " --channel " + str(channel_n) + " --put rand.txt", shell=True)
                print("File transferred")
            except:
                print("[ERR] Couldn't transfer file to", mac_addr, "@", str(channel_n))
        
        else:
            ser.write(b'ERR_UNKNOWN_CMD\n')
    except serial.serialutil.SerialException:
        print("SerialException occured. Possibly disconnected?")
        wait_for_device_connection()
    except:
        print("Unknown Error occured")
