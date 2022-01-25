/**
 *  Name:         PnoiPhone index.js
 *  Description:  Cordova JS Frontend interface code to PnoiPhone Wi-Fi Server
 *  Author:       Syed Fahad
 *  Created:      Jan 2022
**/

const ServerPath = "http://192.168.4.1:5000/";

// bind events
document.addEventListener('deviceready', onDeviceReady, false);
commandButton.addEventListener('touchend', toggleRecording, false);
transferButton.addEventListener('touchend', transferRecording, false);

// display a message on screen
function display(msg)
{
	document.getElementById("log").innerHTML += msg + "<br/>";
}

function showError(err)
{
	display(err);
	alert(err);
}

// Send command to server, execute success() or failure() on success or failure,
// respectively
function sendCmd(cmd, success, failure)
{
	cordova.plugin.http.sendRequest(
		ServerPath + cmd, // server address + command
		{ method: 'get' },
		function (response) { if (response.status == 200) success(response.data); },
		function (err) { failure(err.error); }
	);
}

// send command to start or stop recording
function toggleRecording()
{
	var startRecording = function ()
	{
		sendCmd("start",
			function(response)
			{ // Command sent successfully
				display("Recording started");
				commandButton.innerHTML = "Stop";
			},
			showError
		);
	};
	var stopRecording = function ()
	{
		sendCmd("stop",
			function(response)
			{ // Command sent successfully
				display("Recording stopped");
				commandButton.innerHTML = "Start";
			},
			showError
		);
	};

	if      (commandButton.innerHTML == "Start") startRecording();
	else if (commandButton.innerHTML == "Stop")  stopRecording();
}

// transfer the recording from server to phone
function transferRecording()
{
	var fileName = "recording.wav";

	// Find path to store the recording. This is different on iOS and Android so handle
	// these cases separately
	var storageLocation = "";

	switch (device.platform)
	{
		case "Android":
			storageLocation = 'file:///storage/emulated/0/Download/';
			break;

		case "iOS":
			storageLocation = cordova.file.documentsDirectory;
			break;

		default:
			showError("Unsupported device platform");
			return;
	}

	var download = function ()
	{
		display("Downloading file to: " + storageLocation + fileName);

		cordova.plugin.http.downloadFile(ServerPath + "transfer", {}, {},
			storageLocation + fileName,
			function(entry)
			{
				// prints the filename
				display("Download complete");
			},
			function(err)
			{
			  showError(err.error);
			}
		);
	};

	// Check if the recording hasn't been stopped yet
	if (commandButton.innerHTML == "Stop")
	{
		// if the recording is still running, confirm if the user wants to stop it
		if (confirm("Do you want to stop the recording?"))
		{
			toggleRecording();
			download();
		}
	}
	else download();
}

function onDeviceReady() // executed when app starts
{
	// Send a blank command to check if connection is established
	sendCmd("",
		function success(data) { display("Connection established. Ready for commands"); },
		function failure(err)
		{
			showError("Error: Can't connect to Pnoi server (" + err + ")");
		}
	);
}

