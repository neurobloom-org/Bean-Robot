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
String serverIp = "http://10.191.218.103:5000";

// --- RAW GPIO PIN DEFINITIONS ---

// Microphone (INMP441) -> Channel 1
#define I2S_MIC_SD  44 // Physical Pin RX0
#define I2S_MIC_SCK 2  // Physical Pin A1
#define I2S_MIC_WS  1  // Physical Pin A0

// Speaker (MAX98357A) -> Channel 0
#define I2S_SPK_DIN  5 // Physical Pin D2
#define I2S_SPK_BCLK 6 // Physical Pin D3
#define I2S_SPK_LRC  7 // Physical Pin D4
// ** CRITICAL: The Speaker's "SD" pin must be wired directly to 3.3V or 5V! **

#define SAMPLE_RATE 16000

AudioGeneratorMP3 *mp3 = NULL;
AudioFileSourceBuffer *buff = NULL;
AudioOutputI2S *out = NULL;
AudioFileSourceHTTPStream *file = NULL;

void setup() {
  Serial.begin(115200);
  
  delay(3000); 
  Serial.println("\n\n=== 🚀 BEAN AI STANDALONE ===");

  Serial.println("[1] Connecting Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\n✅ Connected to WiFi!");
  
  Serial.println("[2] Booting Microphone on Channel 1...");
  installMic(); // We only have to call this ONE time now!

  Serial.println("\n=== SYSTEM READY ===");
  Serial.println("Type 'r' and press Enter in the bar above to record.");
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
      Serial.println("✅ Playback Finished");
      
      if (file) { delete file; file = NULL; }
      if (buff) { delete buff; buff = NULL; }
      if (out)  { delete out; out = NULL; }
      if (mp3)  { delete mp3; mp3 = NULL; }
      
      Serial.println("Type 'r' to record again.");
    }
  }
}

// --- MIC FUNCTIONS ---
void installMic() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, 
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Grounded L/R
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8, 
    .dma_buf_len = 64, 
    .use_apll = false
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_MIC_SCK,
    .ws_io_num = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD
  };
  
  i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_1);
}

void recordAndSend() {
  if (mp3 && mp3->isRunning()) {
    Serial.println("Wait for speaker to finish!");
    return;
  }

  Serial.println("\n🎙️ Recording 3 Seconds...");
  
  const int rec_size = SAMPLE_RATE * 2 * 3; 
  int16_t *tx_buffer = (int16_t *)malloc(rec_size);
  if (!tx_buffer) { Serial.println("❌ RAM Allocation Failed"); return; }

  int32_t raw_chunk[64];
  size_t bytes_read;
  int samples_collected = 0;
  int total_samples = rec_size / 2;
  bool mic_alive = false;

  while (samples_collected < total_samples) {
    i2s_read(I2S_NUM_1, raw_chunk, sizeof(raw_chunk), &bytes_read, portMAX_DELAY);
    int samples_in_chunk = bytes_read / 4;

    for (int i = 0; i < samples_in_chunk; i++) {
      if (raw_chunk[i] != 0) mic_alive = true; 

      if (samples_collected < total_samples) {
        tx_buffer[samples_collected++] = (int16_t)(raw_chunk[i] >> 14); 
      }
    }
  }

  if (!mic_alive) {
    Serial.println("❌ WARNING: Mic data is all ZEROS. Check your A1, A0, RX0 wiring!");
    free(tx_buffer);
    return;
  } else {
    Serial.println("✅ Mic Data Captured! Sending to Server...");
  }

  HTTPClient http;
  http.setTimeout(30000); 
  http.begin(serverIp + "/chat");
  http.addHeader("Content-Type", "application/octet-stream");
  
  int code = http.POST((uint8_t *)tx_buffer, rec_size);
  http.end();
  free(tx_buffer);

  if (code == 200) {
    Serial.println("✅ Server Responded 200 OK! Playing Audio...");
    playResponse();
  } else {
    Serial.printf("❌ Server Error Code: %d\n", code);
  }
}

// --- SPEAKER FUNCTIONS ---
void playResponse() {
  out = new AudioOutputI2S();
  out->SetPinout(I2S_SPK_BCLK, I2S_SPK_LRC, I2S_SPK_DIN);
  
  mp3 = new AudioGeneratorMP3();
  file = new AudioFileSourceHTTPStream((serverIp + "/reply.mp3").c_str());
  buff = new AudioFileSourceBuffer(file, 8192); 
  
  mp3->begin(buff, out);
}