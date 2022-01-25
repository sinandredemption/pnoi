#!/usr/bin/env python3
"""
Name:        pnoi_interface.py
Description: Simple python interface for PnoiPhone that listens to commands via Bluetooth
             serial using PyBluez library
Author:      Syed Fahad (sydfhd at gmail.com)
Date:        Jan 2022
"""
import os
import subprocess
import bluetooth
import logging
import gpiozero
from time import sleep

# Declerations
colorOFF          = (0,0,0)
colorReady        = (1,1,0)
colorRecording    = (1,0,1)
colorTransferring = (0,0,1)
colorError        = (0,1,1)

# Two LEDs are used to denote the current connection state and command status
cmdStatusLED = gpiozero.RGBLED(red=23, green=27, blue=22)

def report_error(msg, blinks=3):
    logging.error(msg) # Log error to logfile

    # Blink the command-status LED with colorError
    global cmdStatusLED

    origColor = cmdStatusLED.color

    for i in range(blinks):
        cmdStatusLED.color = colorError
        sleep(0.2)
        cmdStatusLED.color = colorOFF
        sleep(0.8)

    cmdStatusLED.color = origColor

# Setup logging
logging.basicConfig(filename='pnoi.log', level=logging.DEBUG)

logging.info("Starting Pnoi server interface...")
logging.debug("working dir = " + os.getcwd())

logging.debug("Setting up LEDs...")

# Turn off both LEDs
cmdStatusLED.color = colorOFF

logging.debug("Running hciconfig hci0 piscan...")

# Make Bluetooth interface discoverable
os.system("sudo hciconfig hci0 piscan")


while True:
    # Setup server connection
    logging.info("Setting up server connection...")

    server_sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
    server_sock.bind(("", bluetooth.PORT_ANY))
    server_sock.listen(1)

    port = server_sock.getsockname()[1]

    uuid = "94f39d29-7d6d-437d-973b-fba39e49d4ee"

    bluetooth.advertise_service(server_sock, "PnoiBTServer", service_id=uuid,
                                service_classes=[uuid, bluetooth.SERIAL_PORT_CLASS],
                                profiles=[bluetooth.SERIAL_PORT_PROFILE],
                               )

    logging.info("Waiting for connection on RFCOMM channel" + str(port))

    client_sock, client_info = server_sock.accept()
    
    # At this point, we're connected to a bluetooth device, so start listening for commands
    logging.info("Listening for commands from" + str(client_info))

    cmdStatusLED.color = colorReady 

    isRecording = False # Prevent transfers while recording 

    try:
        while True:
            data = client_sock.recv(1024)

            if not data:
                break

            logging.debug("Received " + data.decode())

            cmd = data.decode().strip()

            if cmd == "CMD_START":
                # Start recording
                logging.info("Starting recording...")

                cmdStatusLED.color = colorRecording 
                os.system("./scripts/record.sh &")
                isRecording = True

            elif cmd == "CMD_STOP":
                # Stop recording
                logging.info("Stopping recording...")

                os.system("killall arecord")

                isRecording = False
                cmdStatusLED.color = colorReady

            elif cmd == "CMD_TRANSFER" and not isRecording:
                cmdStatusLED.color = colorTransferring 

                mac_addr = client_info[0]
                logging.info("Transferring file to " + str(mac_addr))

                try:
                    logging.debug("Finding OBEX channel number on host...")
                    find_obex_channel = subprocess.run("./scripts/find_obex_channel.sh "
                                                        + mac_addr, shell=True,
                                                        capture_output=True
                                                      )

                    find_obex_channel.check_returncode() # Handled below via CalledProcessError

                    channel_n = int(find_obex_channel.stdout)

                    logging.debug("Sending request for file transfer to"
                            + str(mac_addr) + "...")
                    os.system("obexftp --nopath --noconn --uuid none --bluetooth "
                            + str(mac_addr) + " --channel " + str(channel_n)
                            + " --put stereo_sample.wav")

                except subprocess.CalledProcessError:
                    report_error("Couldn't find OBEX channel on device")

                except:
                    report_error("Unknown error occured")

                finally:
                    cmdStatusLED.color = colorReady
    except OSError:
        report_error("Unknown OSError", blinks=1)
        pass

    logging.info("Disconnected. Ending session...")

    cmdStatusLED.color = colorOFF

    client_sock.close()
    server_sock.close()

    logging.info("All done.")
