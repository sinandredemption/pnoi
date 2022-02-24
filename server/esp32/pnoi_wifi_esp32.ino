#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "WiFi.h"
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <driver/i2s.h>

#define BUFFER_SIZE (32*1024)
#define SAMPLE_RATE 16000
#define BITS_PER_SAMPLE 32
#define NUM_CHANNELS 2
#define BYTE_RATE (SAMPLE_RATE * (BITS_PER_SAMPLE / 8)) * NUM_CHANNELS
const i2s_port_t I2S_PORT = I2S_NUM_0;
const int WAVE_HEADER_SIZE = 44;

bool keepRecording = false;

#define SERVER_PORT 5000
const char* ssid = "pnoi_esp32";
const char* password = "pnoiphone";

#define FEEDBACK_PIN 13

WebServer server(SERVER_PORT);

void vibrate_and_blink()
{
  digitalWrite(FEEDBACK_PIN, HIGH);
  vTaskDelay(250 / portTICK_PERIOD_MS);
  digitalWrite(FEEDBACK_PIN, LOW);
  vTaskDelay(500 / portTICK_PERIOD_MS);
}

void buzz_once(void*)
{
  vibrate_and_blink();
  vTaskDelete(NULL);
}

void error_loop_forever()
{
  while (1) vibrate_and_blink();
}

void generate_wav_header(char* wav_header, uint32_t wav_size, uint32_t sample_rate){
    // See this for reference: http://soundfile.sapp.org/doc/WaveFormat/
    uint32_t file_size = wav_size + WAVE_HEADER_SIZE - 8;
    uint32_t byte_rate = BYTE_RATE;

    const char set_wav_header[] = {
        'R','I','F','F', // ChunkID
        file_size, file_size >> 8, file_size >> 16, file_size >> 24, // ChunkSize
        'W','A','V','E', // Format
        'f','m','t',' ', // Subchunk1ID
        0x10, 0x00, 0x00, 0x00, // Subchunk1Size (16 for PCM)
        0x01, 0x00, // AudioFormat (1 for PCM)
        0x02, 0x00, // NumChannels (2 channel)
        sample_rate, sample_rate >> 8, sample_rate >> 16, sample_rate >> 24, // SampleRate
        byte_rate, byte_rate >> 8, byte_rate >> 16, byte_rate >> 24, // ByteRate
        0x02, 0x00, // BlockAlign
        0x20, 0x00, // BitsPerSample (16 bits)
        'd','a','t','a', // Subchunk2ID
        wav_size, wav_size >> 8, wav_size >> 16, wav_size >> 24, // Subchunk2Size
    };

    memcpy(wav_header, set_wav_header, sizeof(set_wav_header));
}

void record(void*)
{
   // Use POSIX and C standard library functions to work with files.
    int flash_wr_size = 0;
    Serial.println("Opening file");

    char wav_header_fmt[WAVE_HEADER_SIZE];
    
    // Write out empty header
    memset(wav_header_fmt, 0, WAVE_HEADER_SIZE);

    // First check if file exists before creating a new file.
    if (SD.exists("/record.wav")) {
        // Delete it if it exists
        SD.remove("/record.wav");
    }

    // Create new WAV file
    File f = SD.open("/record.wav", FILE_WRITE);
    if (!f) {
        Serial.println("Failed to open file for writing");
        return;
    }

    // Write the header to the WAV file
    f.write((const uint8_t *)wav_header_fmt, WAVE_HEADER_SIZE);

    static char i2s_readraw_buff[BUFFER_SIZE];
    size_t bytes_read;
    // Start recording
    keepRecording = true;
    while (keepRecording) {
        // Read the RAW samples from the microphone
        i2s_read(I2S_PORT, (char *)i2s_readraw_buff, BUFFER_SIZE, &bytes_read, portMAX_DELAY);
        // Write the samples to the WAV file
        f.write((const uint8_t *)i2s_readraw_buff, bytes_read);
        flash_wr_size += bytes_read;
    }

    // Write out the header at the beginning
    generate_wav_header(wav_header_fmt, flash_wr_size, SAMPLE_RATE);
    f.seek(0);
    f.write((const unsigned char*)wav_header_fmt, WAVE_HEADER_SIZE);
    
    Serial.print("Recording done!");
    f.close();
    Serial.println(" File written on SDCard.");

    vTaskDelete(NULL);
}

void start_recording() {
  Serial.println("Recieved request to START recording...");
  server.send(200, "text/plain", "OK");

  // Provide haptic feedback
  xTaskCreate(buzz_once, "buzz_once", 1000, NULL, 1, NULL);
  
  xTaskCreate(record, "start_recording", BUFFER_SIZE + 1000, NULL, 1, NULL);
}

void stop_recording() {
  Serial.println("Recieved request to STOP recording...");
  server.send(200, "text/plain", "OK");
  
  // Provide haptic feedback
  xTaskCreate(buzz_once, "buzz_once", 1000, NULL, 1, NULL);
  
  keepRecording = false;
}


bool transfer_recording() {
    // Provide haptic feedback
    xTaskCreate(buzz_once, "buzz_once", 1000, NULL, 1, NULL);
    
    String dataType = "audio/x-wav";
    
    File dataFile = SD.open("/record.wav");
    
    if (!dataFile) return false;
  
    //if (server.hasArg("download")) dataType = "application/octet-stream";

    Serial.print("Sending " + dataType + " to client...");
    if (server.streamFile(dataFile, dataType) != dataFile.size()) {
      Serial.println("Sent less data than expected!");
    }
  
    dataFile.close();
    Serial.println(" DONE");
    
    return true;
}

void handleNotFound() {
  String message = "\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.print(message);
}

void handle_root() {
    Serial.println("[INFO] New client connected");
    server.send(200, "text/plain", "READYOK");
}

void setup(void) {
  pinMode(FEEDBACK_PIN, OUTPUT);
  digitalWrite(FEEDBACK_PIN, LOW);
  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.print("\n");
  Serial.print("Configuring I2S...");
  esp_err_t err;

  // The I2S config as per the example
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), // Receive, not transfer
      .sample_rate = SAMPLE_RATE,                         // 16KHz
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // could only get it to work with 32bits
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // although the SEL config should be left, it seems to transmit on right
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,     // Interrupt level 1
      .dma_buf_count = 4,                           // number of buffers
      .dma_buf_len = 8                              // 8 samples per buffer (minimum)
  };

  // The pin config as per the setup
  const i2s_pin_config_t pin_config = {
      .bck_io_num = 14,   // BCKL
      .ws_io_num = 15,    // LRCL
      .data_out_num = -1, // not used (only for speakers)
      .data_in_num = 32   // DOUT
  };

  // Configuring the I2S driver and pins.
  // This function must be called before any I2S driver read/write operations.
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf(" FAILED installing driver: %d\n", err);
    error_loop_forever();
  }
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf(" FAILED setting pin: %d\n", err);
    error_loop_forever();
  }
  Serial.println(" DONE");

  Serial.printf("Starting WiFi AP = %s", ssid);
  WiFi.softAP(ssid, password);
  Serial.println(" DONE");

  server.on("/", HTTP_GET, handle_root);
  server.on("/start", HTTP_GET, start_recording);
  server.on("/stop", HTTP_GET, stop_recording);
  server.on("/transfer", HTTP_GET, transfer_recording);
  server.onNotFound(handleNotFound);

  Serial.printf("Starting HTTP server at port = %d...", SERVER_PORT);
  server.begin();
  Serial.println(" DONE");

  Serial.print("Initializing SD Card...");
  if (SD.begin()) {
    Serial.println(" DONE");
  } else {
    Serial.println(" FAILED");
    error_loop_forever();
  }
}

void loop() {
    server.handleClient();
    delay(2);//allow the cpu to switch to other tasks
}
