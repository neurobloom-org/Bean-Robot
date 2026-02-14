#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "AudioFileSourceHTTPStream.h"

// --- WI-FI ---
const char* ssid = "Randil's A35";
const char* password = "theonepieceisreal";
String serverIp = "http://10.159.192.103:5000"; 

// --- PIN DEFINITIONS ---
// Mic (INMP441) - UPDATED
#define I2S_MIC_SCK 14
#define I2S_MIC_WS  15
#define I2S_MIC_SD  35  // <--- MOVED TO PIN 35 (Safe Pin)

// Speaker (MAX98357A) - UNTOUCHED (Keep as is!)
#define I2S_SPK_BCLK 26
#define I2S_SPK_LRC  25
#define I2S_SPK_DIN  33 
#define I2S_SPK_SD   27 

#define SAMPLE_RATE 16000

AudioGeneratorMP3 *mp3;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;
AudioFileSourceHTTPStream *file;

void setup() {
  Serial.begin(115200);
  Serial.println("--- BEAN FINAL FIX ---");

  // 1. Wake up Speaker Amp (Do not touch this!)
  pinMode(I2S_SPK_SD, OUTPUT);
  digitalWrite(I2S_SPK_SD, HIGH); 

  // 2. Connect Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected to WiFi");
  
  installMic();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r') {
      recordAndSend();
    }
  }

  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) { 
      mp3->stop(); 
      Serial.println("Playback Finished");
      installMic(); // Reset Mic
    }
  }
}

// --- MIC FUNCTIONS ---
void installMic() {
  if (mp3) { delete mp3; mp3 = NULL; } 
  if (out) { delete out; out = NULL; }
  i2s_driver_uninstall(I2S_NUM_0);

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, 
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, // Ensure L/R pin is Grounded!
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8, .dma_buf_len = 64, .use_apll = false
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_MIC_SCK,
    .ws_io_num = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD
  };
  
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void recordAndSend() {
  Serial.println("Recording...");
  
  const int rec_size = SAMPLE_RATE * 2 * 3; 
  int16_t *tx_buffer = (int16_t *)malloc(rec_size);
  if (!tx_buffer) { Serial.println("RAM Fail"); return; }

  int32_t raw_chunk[64];
  size_t bytes_read;
  int samples_collected = 0;
  int total_samples = rec_size / 2;
  
  // Debug check for "Dead Mic"
  bool mic_alive = false;

  while (samples_collected < total_samples) {
    i2s_read(I2S_NUM_0, raw_chunk, sizeof(raw_chunk), &bytes_read, portMAX_DELAY);
    int samples_in_chunk = bytes_read / 4;

    for (int i = 0; i < samples_in_chunk; i++) {
      if (raw_chunk[i] != 0) mic_alive = true; // Check for life

      if (samples_collected < total_samples) {
        // Gain >> 10 (Loud)
        // NEW LINE (Clean Audio):
        tx_buffer[samples_collected++] = (int16_t)(raw_chunk[i] >> 14);
      }
    }
  }

  if (!mic_alive) {
    Serial.println("WARNING: Mic data is all ZEROS. Check Wiring.");
  } else {
    Serial.println("Mic Data Detected! Sending...");
  }

  HTTPClient http;
  http.setTimeout(15000); 
  http.begin(serverIp + "/chat");
  http.addHeader("Content-Type", "application/octet-stream");
  int code = http.POST((uint8_t *)tx_buffer, rec_size);
  http.end();
  free(tx_buffer);

  if (code == 200) {
    playResponse();
  } else {
    Serial.printf("Error: %d\n", code);
  }
}

// --- SPEAKER FUNCTIONS ---
void playResponse() {
  i2s_driver_uninstall(I2S_NUM_0); 

  out = new AudioOutputI2S();
  out->SetPinout(I2S_SPK_BCLK, I2S_SPK_LRC, I2S_SPK_DIN);
  
  mp3 = new AudioGeneratorMP3();
  file = new AudioFileSourceHTTPStream((serverIp + "/reply.mp3").c_str());
  buff = new AudioFileSourceBuffer(file, 8192); 
  mp3->begin(buff, out);
}