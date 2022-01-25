/**
 *  Name:        PnoiPhone index.js
 *  Description: Cordova JS Backend code for PnoiPhone app
 *  Author:      Syed Fahad
 *  Created:     Jan 2022
**/

var app = {
    macAddress: "",
    chars: "",

    initialize: function()
	{
        this.bindEvents();
        console.log("Starting PnoiPhone app");
    },
	
    // bind any events that are required on startup to listeners:
    bindEvents: function()
	{
        document.addEventListener('deviceready', this.onDeviceReady, false);
        connectButton.addEventListener('touchend', app.toggleConnection, false);
        commandButton.addEventListener('touchend', app.toggleRecording, false);
		transferButton.addEventListener('touchend', app.transferRecording, false);
    },

    onDeviceReady: function()
	{
		app.display("Finding all available Bluetooth devices...");

		var displayDevices = function (devices)
		{
			for (const device of devices)
			{
				var opt = document.createElement('option');
				opt.value = device['address'];
				opt.innerHTML = device['name'];
				
				document.getElementById("deviceSelect").appendChild(opt);
			}
		}
		
		// Called by bluetoothSerial.isEnabled() below if Bluetooth is on
        var listPorts = function()
		{
            // List the available BT ports:
            bluetoothSerial.list(
					displayDevices,
                    function(error)   { app.display(JSON.stringify(error)); }
            );

			// TODO Add ability to display unpaired devices as well
        }

        // Called by bluetoothSerial.isEnabled() below if Bluetooth is off
        var notEnabled = function() { app.display("Bluetooth is not enabled."); }

         // check if Bluetooth is on:
        bluetoothSerial.isEnabled(
			listPorts, // BT on
            notEnabled // BT off
        );

		app.display("Completed device discovery");

    },
	
    // Connects if not connected, and disconnects if connected:
    toggleConnection: function()
	{
        var connect = function ()
		{
			app.macAddress = document.getElementById("deviceSelect").value;
            app.clear();
            app.display("Attempting to connect to " + app.macAddress + "... "
                + "Make sure the serial port is open on the target device.");
			
			connectButton.disabled = true;

            // attempt to connect:
            bluetoothSerial.connect(
                app.macAddress,  // device to connect to
                app.openPort,    // start listening if you succeed
                app.showError    // show the error if you fail
            );

			connectButton.disabled = false;
        };

        var disconnect = function ()
		{
            
			if (confirm("Are you sure you want to disconnect?"))
			{
				app.display("attempting to disconnect");
				
				connectButton.disabled = true;
				
				bluetoothSerial.disconnect(
					app.closePort,     // stop listening to the port
					app.showError      // show the error if you fail
				);

				connectButton.disabled = false;
			}
        };

        bluetoothSerial.isConnected(
			disconnect, // if connected, disconnect
			connect		// if disconnected, connect
		);
    },

	// send command to start or stop recording
    toggleRecording: function()
	{
	    var startRecording = function ()
		{
		    bluetoothSerial.write("CMD_START\n",
			    function() 
				{ // Command sent successfully
				    app.display("Starting recording...");
				    commandButton.innerHTML = "Stop";
			    },
			    app.showError // Failed
		    );
	    };

	    var stopRecording = function ()
		{
		    bluetoothSerial.write("CMD_STOP\n",
			    function()
				{ // Command sent successfully
				    app.display("Stopping recording");
				    commandButton.innerHTML = "Start";
			    },
			    app.showError
		    );
	    };

	    if      (commandButton.innerHTML == "Start") startRecording();
	    else if (commandButton.innerHTML == "Stop")  stopRecording();
    },
    
	transferRecording: function ()
	{
		var sendTransferCommand = function ()
		{
			bluetoothSerial.write("CMD_TRANSFER\n",
				function()
				{ // Success
					app.display("Transferring...");
				},
				app.showError
			);
		};

		// Check if the recording hasn't been stopped yet
		if (commandButton.innerHTML == "Stop")
		{
			if (confirm("Do you want to stop the recording?"))
			{
					app.toggleRecording();
					sendTransferCommand();
			}
		}
		else sendTransferCommand();
	},

    // Subscribes to a Bluetooth serial listener and updates the connect button
    openPort: function()
	{
        app.display("Connected to: " + app.macAddress);
        
		// set up a listener to listen for newlines
        // and display any new data that's come in since
        // the last newline:
        bluetoothSerial.subscribe('\n',
			function (data)
			{
            	app.clear();
            	app.process(data);
        	}
		);
		
        // change the button's name:
        connectButton.innerHTML = "Disconnect";
    },

    // Unsubscribes from any Bluetooth serial listener and updates the connect button 
    closePort: function() {
        // if you get a good Bluetooth serial connection:
        app.display("Disconnected from: " + app.macAddress);
        // change the button's name:
        connectButton.innerHTML = "Connect";
        // unsubscribe from listening:
        bluetoothSerial.unsubscribe(
                function (data)
				{
                    app.display(data);
                },
                app.showError
        );
    },
	
    showError: function(error) { app.display(error); },

	// Processes and displays data recieved from the server
	process: function(data) { app.display(data); },
	
    // appends @message to the message div:
    display: function(message)
	{
        var display = document.getElementById("message"), 
            lineBreak = document.createElement("br"),     
            label = document.createTextNode(message);

        display.appendChild(lineBreak);          // add a line break
        display.appendChild(label);              // add the message node
    },
	
    // Clears the message div:
    clear: function()
	{
        var display = document.getElementById("message");
        display.innerHTML = "";
    }
};      // end of app

