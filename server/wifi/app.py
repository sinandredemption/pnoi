from flask import Flask, send_file
from flask_cors import CORS
import os
import logging
import subprocess

app = Flask(__name__)
CORS(app)

isRecording = False

# setup logging
logging.basicConfig(filename='pnoi.log', level=logging.DEBUG)

logging.info("Pnoi Flask Server running with working dir = " + os.getcwd())

@app.route("/")
def hello_world():
    return "READY OK", 200

@app.route('/start')
def start_recording():
    global isRecording

    # Start recording
    logging.info("Starting recording...")

    # cmdStatusLED.color = colorRecording
    # os.system("./scripts/record.sh &")
    isRecording = True

    os.system("echo '$RANDOM' > record.sample.txt")
    
    return 'OK', 200 

@app.route('/stop')
def stop_recording():
    # Stop recording
    logging.info("Stopping recording...")
    
    os.system("killall arecord")

    isRecording = False
    # cmdStatusLED.color = colorReady

    return 'OK', 200

@app.route('/transfer')
def transfer_file():
    # Transfer last recording
    return send_file('record.sample.txt')
