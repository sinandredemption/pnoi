#!/bin/bash

cd /home/pi/pnoiserver/

while [ ! -f STOP ] # Keep on running until explicitly told to stop 
do	
	sudo python3 /home/pi/pnoiserver/interface.py
	sleep 3
done
