#include <Adafruit_GFX.h>    
#include <Adafruit_ST7735.h> 
#include <SPI.h>
#include <Audio.h>  
#include <SD.h>
#include <FS.h>
#include "logo.h" 
#include "animations.h" 

// --- TRUE INTERNAL PINS FOR NANO ESP32 ---
#define TFT_CS     21   
#define TFT_DC     17   
#define TFT_RST    10   
#define SD_CS      3    
#define I2S_DOUT   5    
#define I2S_BCLK   6    
#define I2S_LRC    7    
#define NAV_BTN    4
#define TOUCH_PIN  14
#define VIBE_PIN   8
#define DISPLAY_PWR 9

// --- COLORS ---
#define C_BLACK      0x0000 
#define C_WHITE      0xFFFF 
#define C_MINT       0x9772 
#define C_CREAM      0xFFD9 
#define C_GREY       0x2124
#define C_RED        0xF800

Adafruit_ST7735 tft = Adafruit_ST7735(&SPI, TFT_CS, TFT_DC, TFT_RST);
Audio audio; 

enum OS_State { HOME, EMERGENCY, FOCUS, MUSIC, BREATHE };
OS_State currentPage = HOME;
OS_State lastPage = BREATHE; 

// UI & Music States
bool locationShared = false;
bool musicPlaying = false;
bool sdMounted = false; // CRASH SHIELD
const char* playlist[] = {"blue.mp3", "brunch.mp3", "ocean.mp3"}; 
int currentTrack = 0;

// Timer & Animation States
long focusSecondsLeft = 25 * 60; 
bool focusActive = false;
unsigned long lastFocusTick = 0;
unsigned long touchStartTime = 0;
unsigned long lastAnimUpdate = 0;

void setup() {
  Serial.begin(115200);
  
  pinMode(DISPLAY_PWR, OUTPUT);
  digitalWrite(DISPLAY_PWR, HIGH);
  pinMode(NAV_BTN, INPUT_PULLUP);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(VIBE_PIN, OUTPUT);

  // 1. Set up CS pins to be completely off by default
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // 2. Start the shared SPI bus
  SPI.begin(48, 47, 38, -1); 
  delay(50);

  // 3. WAKE UP SCREEN FIRST
  tft.initR(INITR_GREENTAB); 
  tft.setRotation(3); 
  tft.fillScreen(C_BLACK);
  tft.drawRGBBitmap(0, 0, (const uint16_t*)neuroLogo, 160, 128);
  delay(500); // Give the screen time to settle

  // 4. WAKE UP SD CARD SECOND
  if (SD.begin(SD_CS, SPI, 10000000)) {
    sdMounted = true;
    Serial.println("✅ SD Mounted AFTER Screen!");
  } else {
    Serial.println("❌ SD Mount Failed - Hardware collision confirmed.");
  }

  // 5. Init Audio 
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(20); 
}
void loop() {
  audio.loop(); 
  handleInputs();
  handleLogic();

  if (currentPage != lastPage) {
    tft.fillScreen(C_BLACK); 
    refreshUI();
    lastPage = currentPage;
  }
}

void handleInputs() {
  static bool prevNav = false;
  bool currentNav = (digitalRead(NAV_BTN) == LOW);
  
  if (currentNav && !prevNav) {
    if(musicPlaying) { audio.stopSong(); musicPlaying = false; }
    currentPage = (OS_State)((currentPage + 1) % 5);
    vibe(30);
    delay(100);
  }
  prevNav = currentNav;

  if (digitalRead(TOUCH_PIN) == HIGH) {
    if (touchStartTime == 0) touchStartTime = millis();
  } else {
    if (touchStartTime != 0) {
      unsigned long dur = millis() - touchStartTime;
      
      if (dur > 800) { // LONG PRESS
        if (currentPage == FOCUS) {
          focusSecondsLeft = 25 * 60;
          focusActive = false;
        } else if (currentPage == MUSIC && sdMounted) { // Protected
          currentTrack = (currentTrack + 1) % 3;
          String path = "/" + String(playlist[currentTrack]);
          audio.connecttoFS(SD, path.c_str());
          musicPlaying = true;
        }
        vibe(150);
      } else { // SINGLE TAP
        if (currentPage == EMERGENCY) locationShared = !locationShared;
        else if (currentPage == FOCUS) focusActive = !focusActive;
        else if (currentPage == MUSIC) {
          if (!musicPlaying && sdMounted) { // Protected
            String path = "/" + String(playlist[currentTrack]);
            audio.connecttoFS(SD, path.c_str());
            musicPlaying = true;
          } else if (sdMounted) {
            audio.pauseResume();
          }
        }
        vibe(50);
      }
      refreshUI();
      touchStartTime = 0;
    }
  }
}

void handleLogic() {
  if (focusActive && currentPage == FOCUS) {
    if (millis() - lastFocusTick >= 1000) {
      if (focusSecondsLeft > 0) focusSecondsLeft--;
      lastFocusTick = millis();
      drawFocus(false); 
    }
  }

  if (currentPage == BREATHE && millis() - lastAnimUpdate > 20) {
    lastAnimUpdate = millis();
    drawBreathe();
  }
}

void refreshUI() {
  switch (currentPage) {
    case HOME:      tft.drawRGBBitmap(0, 0, (const uint16_t*)Full_eye, 160, 128); break;
    case EMERGENCY: drawEmergency(); break;
    case FOCUS:     drawFocus(true); break; 
    case MUSIC:     drawMusic(); break;
    case BREATHE:   tft.fillScreen(C_BLACK); break;
  }
  drawPageIndicators();
}

void drawMusic() {
  tft.fillRoundRect(10, 10, 140, 95, 15, C_GREY);
  tft.fillCircle(45, 52, 35, C_BLACK);
  tft.drawCircle(45, 52, 30, 0x4208); 
  tft.fillCircle(45, 52, 10, musicPlaying ? C_MINT : C_CREAM); 
  
  tft.setTextColor(C_WHITE); tft.setCursor(85, 35); tft.print("Track:");
  tft.setTextColor(C_MINT); tft.setCursor(85, 50); tft.print(playlist[currentTrack]);
  
  tft.setTextColor(C_WHITE); tft.setCursor(85, 80);
  tft.print(musicPlaying ? "I< || >I" : "I< >> >I");
  
  if (!sdMounted) {
    tft.setTextColor(C_RED); tft.setCursor(85, 65); tft.print("NO SD!");
  }
}

void drawBreathe() {
  unsigned long elapsed = millis() % 8000; 
  int pulse;
  const char* msg;

  if (elapsed < 3000) { pulse = map(elapsed, 0, 3000, 15, 45); msg = "INHALE..."; }
  else if (elapsed < 5000) { pulse = 45; msg = "  HOLD..."; }
  else { pulse = map(elapsed, 5000, 8000, 45, 15); msg = "EXHALE..."; }

  tft.fillCircle(80, 50, 47, C_BLACK); 
  tft.drawCircle(80, 50, pulse, C_MINT);
  tft.drawCircle(80, 50, pulse-1, C_CREAM);
  
  tft.fillRect(40, 105, 80, 15, C_BLACK);
  tft.setTextColor(C_MINT);
  tft.setCursor(55, 110); tft.print(msg);
}

void drawEmergency() {
  tft.fillRoundRect(5, 5, 150, 105, 12, C_CREAM);
  tft.setTextColor(C_BLACK);
  tft.setCursor(15, 18); tft.print("EMERGENCY CARE");
  tft.drawFastHLine(15, 30, 130, C_GREY);
  tft.setCursor(15, 45); tft.print("Contact: Mom");
  tft.setCursor(15, 60); tft.print("No: 0760764170");
  tft.fillRoundRect(115, 80, 32, 16, 8, C_GREY);
  tft.fillCircle(locationShared ? 139 : 123, 88, 6, locationShared ? C_MINT : C_RED);
}

void drawFocus(bool fullDraw) {
    int m = focusSecondsLeft / 60;
    int s = focusSecondsLeft % 60;
    if (fullDraw) tft.drawCircle(80, 58, 42, C_GREY);
    tft.fillRect(55, 48, 50, 22, C_BLACK); 
    tft.setTextSize(2);
    tft.setTextColor(focusActive ? C_MINT : C_WHITE);
    tft.setCursor(52, 51);
    if(m<10) tft.print("0"); tft.print(m); tft.print(":");
    if(s<10) tft.print("0"); tft.print(s);
    tft.setTextSize(1);
}

void drawPageIndicators() {
  for (int i = 0; i < 5; i++) {
    tft.fillCircle(55 + (i * 12), 122, (i == currentPage ? 3 : 1), C_WHITE); 
  }
}

void vibe(int ms) {
  digitalWrite(VIBE_PIN, HIGH);
  delay(ms);
  digitalWrite(VIBE_PIN, LOW);
}