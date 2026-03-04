#include <Adafruit_GFX.h>    
#include <Adafruit_ST7735.h> 
#include <SPI.h>
#include <driver/i2s.h>

// --- RAW GPIO PINS (The True Map) ---
// Display
#define TFT_RST   10
#define TFT_DC    17
#define TFT_CS    21
#define TFT_MOSI  38
#define TFT_SCLK  48

// Speaker Amp (Output)
#define AMP_DIN   5
#define AMP_BCLK  6
#define AMP_LRC   7

// Microphone (Input)
#define MIC_WS    1
#define MIC_SCK   2
#define MIC_SD    44

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void setup() {
  Serial.begin(115200);

  // 1. Initialize Screen
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW); delay(100);
  digitalWrite(TFT_RST, HIGH); delay(100);
  
  tft.initR(INITR_BLACKTAB); 
  tft.setRotation(1); // Widescreen
  tft.fillScreen(ST77XX_BLACK);
  
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("A/V TEST");

  // 2. Initialize Speaker (Using I2S Channel 0)
  i2s_config_t speaker_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64
  };
  i2s_pin_config_t speaker_pins = {
    .bck_io_num = AMP_BCLK,
    .ws_io_num = AMP_LRC,
    .data_out_num = AMP_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_0, &speaker_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &speaker_pins);

  // 3. Initialize Microphone (Using I2S Channel 1)
  i2s_config_t mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // Mics usually need 32-bit reads
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64
  };
  i2s_pin_config_t mic_pins = {
    .bck_io_num = MIC_SCK,
    .ws_io_num = MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_SD
  };
  i2s_driver_install(I2S_NUM_1, &mic_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &mic_pins);

  // Ready message
  tft.setCursor(10, 50);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.println("READY!");
  Serial.println("System Online. Make some noise!");
}

void loop() {
  int32_t sample = 0;
  size_t bytes_read;
  int32_t max_volume = 0;

  // Listen to the room for a tiny fraction of a second
  for (int i = 0; i < 100; i++) {
    i2s_read(I2S_NUM_1, &sample, sizeof(sample), &bytes_read, portMAX_DELAY);
    
    // Convert the raw 32-bit mic data into a readable volume number
    sample = sample >> 14; 
    
    if (abs(sample) > max_volume) {
      max_volume = abs(sample);
    }
  }

  // If the volume spikes (like a clap)
  if (max_volume > 1500) { 
    Serial.println("Sound Detected!");
    
    // Draw visual feedback
    tft.fillRect(10, 80, 100, 30, ST77XX_RED);
    tft.setCursor(15, 85);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("CLAP!");

    // Play the audio feedback
    playBeep(); 

    // Clear the screen text after half a second
    delay(500); 
    tft.fillRect(10, 80, 100, 30, ST77XX_BLACK); 
  }
}

// Function to generate a simple electronic beep
void playBeep() {
  int16_t beep_sample = 0;
  size_t bytes_written;
  for(int i = 0; i < 800; i++){ // Length of the beep
    beep_sample = (i % 20 < 10) ? 4000 : -4000; // Pitch and Volume
    i2s_write(I2S_NUM_0, &beep_sample, sizeof(beep_sample), &bytes_written, portMAX_DELAY);
  }
}