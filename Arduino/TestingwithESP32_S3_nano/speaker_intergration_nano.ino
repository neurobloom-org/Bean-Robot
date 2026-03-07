#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Audio.h>

#define SD_CS      3    
#define TFT_CS     21   

#define I2S_DOUT   5    
#define I2S_BCLK   6    
#define I2S_LRC    7    

Audio audio;

void setup() {
  Serial.begin(115200);
  delay(1000); 

  // Mute the screen
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH); 

  SPI.begin(48, 47, 38, SD_CS); 

  // Mount SD Card at High Speed
  if (!SD.begin(SD_CS, SPI, 16000000)) { 
    Serial.println("❌ FATAL: SD Card Mount Failed!");
    return;
  }
  
  // Init Audio
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  
  // CRANK IT TO MAX (usually 21 is max for this library)
  audio.setVolume(20); 

  Serial.println("Attempting to play /blue.mp3...");
  audio.connecttoFS(SD, "/blue.mp3");
}

void loop() {
  audio.loop(); 
}