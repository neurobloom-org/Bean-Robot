#include <Adafruit_GFX.h>    
#include <Adafruit_ST7735.h> 
#include <SPI.h>
#include <Audio.h>  
#include <Wire.h>
#include <NimBLEDevice.h> 
#include <WiFi.h>         
#include <TinyGPS++.h> 
#include <HTTPClient.h> // NEW: For sending the WhatsApp request!

// --- WHATSAPP CONFIG (CallMeBot) ---
// Get your API key by sending "I allow callmebot to send me messages" to +34 644 47 94 99 on WhatsApp
String whatsappPhone = "+12345678900"; // Replace with your phone number (include country code, no +)
String whatsappApiKey = "123456";      // Replace with your API key from the bot
// -----------------------------------

// --- SHARED SPI PINS ---
#define TFT_CS     21   
#define TFT_DC     17   
#define TFT_RST    10   

// --- AUDIO & CONTROL PINS ---
#define I2S_DOUT   5    
#define I2S_BCLK   6    
#define I2S_LRC    7    
#define NAV_BTN    4
#define TOUCH_PIN  14
#define VIBE_PIN   8
#define DISPLAY_PWR 9

// --- GYRO PINS (MPU6050) ---
#define GYRO_SDA   11    
#define GYRO_SCL   12    
#define MPU_ADDR   0x68

// --- GPS PINS (Nano ESP32) ---
#define GPS_RX_PIN 43 // Physical pin D1 (TX1) on Nano ESP32 is our RX listener!
#define GPS_TX_PIN -1 // Disabled
HardwareSerial GPS_Serial(1);
TinyGPSPlus gps;

// --- COLORS ---
#define C_BLACK      0x0000 
#define C_WHITE      0xFFFF 
#define C_MINT       0x9772 
#define C_CREAM      0xFFD9 
#define C_GREY       0x2124
#define C_RED        0xF800
#define EYE_COLOR    C_MINT 

// --- BLE & WIFI GLOBALS ---
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
const String HARDCODED_PIN = "1234";

bool bleDataReceived = false;
String receivedSSID = "";
String receivedPASS = "";
String receivedUserID = "";

Adafruit_ST7735 tft = Adafruit_ST7735(&SPI, TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(160, 128); 
Audio audio; 

enum OS_State { PAIRING, HOME, EMERGENCY, FOCUS, MUSIC, BREATHE };
OS_State currentPage = PAIRING; 
OS_State lastPage = BREATHE; 

// UI & Music States
bool locationShared = false;
bool musicPlaying = false;
const char* liveStreamURL = "http://ice1.somafm.com/groovesalad-128-mp3"; 
String sysLog = "Ready.";
bool forceUIRefresh = false; 

// Timer & Animation States
long focusSecondsLeft = 25 * 60; 
bool focusActive = false;
unsigned long lastFocusTick = 0;
unsigned long touchStartTime = 0;
unsigned long lastAnimUpdate = 0;
unsigned long lastEqUpdate = 0;

// === EMOTION ENGINE GLOBALS ===
enum Emotion { NORMAL, HAPPY, ANGRY, CONFUSED, SCARED, SLEEPY };
Emotion currentEmotion = NORMAL;
unsigned long lastInteraction = 0;
int frameCount = 0;
long nextBlink = 0;
bool isBlinking = false; 
int blinkHeight = 55;
float gyroOffsetX = 0, gyroOffsetY = 0; 
int16_t AcX, AcY, AcZ;

class MyCallbacks: public NimBLECharacteristicCallbacks {
public: 
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        String payload = String(pCharacteristic->getValue().c_str());
        Serial.println("Received BLE Data: " + payload);
        
        int p1 = payload.indexOf('|');
        int p2 = payload.indexOf('|', p1 + 1);
        int p3 = payload.indexOf('|', p2 + 1);

        if (p1 > -1 && p2 > -1 && p3 > -1) {
            String pinAttempt = payload.substring(0, p1);
            if (pinAttempt == HARDCODED_PIN) {
                Serial.println("✅ PIN ACCEPTED!");
                receivedSSID = payload.substring(p1 + 1, p2);
                receivedPASS = payload.substring(p2 + 1, p3);
                receivedUserID = payload.substring(p3 + 1);
                bleDataReceived = true; 
            } else {
                Serial.println("❌ INVALID PIN ATTACK.");
            }
        } else {
            Serial.println("❌ INVALID DATA FORMAT.");
        }
    }
};

void setupBLE() {
  NimBLEDevice::init("BEAN_ROBOT");
  NimBLEServer *pServer = NimBLEDevice::createServer();
  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  NimBLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         NIMBLE_PROPERTY::READ |
                                         NIMBLE_PROPERTY::WRITE
                                       );
  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println("📻 BLE Broadcasting as 'BEAN_ROBOT'...");
}

// ==========================================
// WHATSAPP SOS FUNCTION
// ==========================================
void sendWhatsAppSOS(float lat, float lng) {
  if (WiFi.status() == WL_CONNECTED) {
    tft.fillRect(15, 80, 130, 20, C_CREAM);
    tft.setCursor(15, 85); tft.setTextColor(C_RED); tft.print("SENDING SOS...");
    
    HTTPClient http;
    // Format coordinates to 6 decimal places for Google Maps link
    String mapLink = "https://maps.google.com/?q=" + String(lat, 6) + "," + String(lng, 6);
    
    // Create the CallMeBot URL
    String url = "https://api.callmebot.com/whatsapp.php?phone=" + whatsappPhone + "&text=%F0%9F%9A%A8+SOS!+I+need+help!+My+location:+" + mapLink + "&apikey=" + whatsappApiKey;
    
    http.begin(url);
    int httpResponseCode = http.GET();
    
    tft.fillRect(15, 80, 130, 20, C_CREAM);
    tft.setCursor(15, 85);
    if (httpResponseCode > 0) {
      Serial.println("WhatsApp Sent! Code: " + String(httpResponseCode));
      tft.setTextColor(C_MINT); tft.print("SOS DELIVERED!");
      locationShared = true;
      vibe(1000); // Long confirmation vibration
    } else {
      Serial.println("WhatsApp Error: " + String(httpResponseCode));
      tft.setTextColor(C_RED); tft.print("SOS FAILED.");
    }
    http.end();
    delay(2000); // Let user read the status
    forceUIRefresh = true;
  }
}

// ==========================================
// GYRO & EMOTION FUNCTIONS
// ==========================================
void setupGyro() {
  Wire.begin(GYRO_SDA, GYRO_SCL);
  Wire.beginTransmission(MPU_ADDR); 
  Wire.write(0x6B); 
  Wire.write(0); 
  Wire.endTransmission(true);
}

void readGyroRaw() {
  Wire.beginTransmission(MPU_ADDR); 
  Wire.write(0x3B); 
  Wire.endTransmission(false);
  Wire.requestFrom((uint16_t)MPU_ADDR, (uint8_t)6, true);
  if (Wire.available() >= 6) {
    AcX = (Wire.read() << 8 | Wire.read());
    AcY = (Wire.read() << 8 | Wire.read());
    AcZ = (Wire.read() << 8 | Wire.read());
  }
}

void drawPixelHeart(int x, int y, int size, uint16_t color) {
  int s = size / 2;
  canvas.fillRoundRect(x-s, y-s, s, s, 4, color); canvas.fillRoundRect(x, y-s, s, s, 4, color);
  canvas.fillRect(x-(s/2), y-(s/2), s, s, color);
  canvas.fillTriangle(x-s, y-(s/4), x+s, y-(s/4), x, y+s+2, color);
}

void drawSwirl(int x, int y, int t) {
  float prevX=-999, prevY=-999;
  for (float i=0; i<40; i+=2.0) { 
      float angle = (i * 0.3) + (t * 0.2);
      float r = i / 1.5;
      int currX = x + (cos(angle)*r); int currY = y + (sin(angle)*r);
      if (prevX!=-999) canvas.drawLine(prevX, prevY, currX, currY, EYE_COLOR);
      prevX = currX; prevY = currY;
  }
}

void runBeanLogic() {
  unsigned long now = millis();
  readGyroRaw();
  
  int16_t safeX = constrain(AcX, -12000, 12000);
  int16_t safeY = constrain(AcY, -12000, 12000);
  int targetX = map(safeY, -12000, 12000, -30, 30);
  int targetY = map(safeX, -12000, 12000, -20, 20);
  
  gyroOffsetX += (targetX - gyroOffsetX) * 0.15;
  gyroOffsetY += (targetY - gyroOffsetY) * 0.15;
  
  long totalForce = abs(AcX) + abs(AcY) + abs(AcZ);
  bool touched = digitalRead(TOUCH_PIN);

  if (touched) {
     if (currentEmotion != HAPPY) { currentEmotion = HAPPY; vibe(100); }
     lastInteraction = now;
  } else if (totalForce > 50000 && currentEmotion != SLEEPY) {
     lastInteraction = now;
     if (random(0,2)==0) { currentEmotion = CONFUSED; } else { currentEmotion = SCARED; }
     vibe(200);
  } else if ((currentEmotion == SCARED || currentEmotion == CONFUSED || currentEmotion == HAPPY) && (now - lastInteraction > 5000)) {
     currentEmotion = NORMAL;
  } else if (currentEmotion != SLEEPY && (now - lastInteraction > 60000)) {
     currentEmotion = SLEEPY;
  }

  canvas.fillScreen(C_BLACK);
  
  if (currentEmotion == NORMAL) {
     if (now > nextBlink && !isBlinking) { isBlinking = true; nextBlink = now + random(1500, 4000); }
     if (isBlinking) { 
       blinkHeight -= 20;
       if (blinkHeight <= 4) { blinkHeight = 4; isBlinking = false; } 
     } else if (blinkHeight < 55) { 
       blinkHeight += 20;
       if (blinkHeight > 55) blinkHeight = 55; 
     }
     
     canvas.fillRoundRect(35+(int)gyroOffsetX, 64+(int)gyroOffsetY-(blinkHeight/2), 35, blinkHeight, 14, EYE_COLOR);
     canvas.fillRoundRect(90+(int)gyroOffsetX, 64+(int)gyroOffsetY-(blinkHeight/2), 35, blinkHeight, 14, EYE_COLOR);
  } 
  else if (currentEmotion == HAPPY) {
     float pulse = (sin(frameCount * 0.2) * 5);
     drawPixelHeart(52, 64, 35+pulse, C_RED); drawPixelHeart(108, 64, 35+pulse, C_RED);
  }
  else if (currentEmotion == SCARED) { 
     int sx = random(-4, 5); int sy = random(-4, 5);
     int cx = 80 + sx; int cy = 64 + sy;
     canvas.fillRoundRect(cx-45, cy-25, 35, 55, 8, EYE_COLOR); canvas.fillTriangle(cx-45, cy-25, cx-10, cy-25, cx-45, cy+10, C_BLACK);
     canvas.fillRoundRect(cx+10, cy-25, 35, 55, 8, EYE_COLOR); canvas.fillTriangle(cx+45, cy-25, cx+10, cy-25, cx+45, cy+10, C_BLACK);
  }
  else if (currentEmotion == CONFUSED) {
     drawSwirl(40, 64, frameCount);
     drawSwirl(120, 64, frameCount);
  }
  else if (currentEmotion == SLEEPY) {
     int h = 4 + (sin(frameCount * 0.08) * 2);
     canvas.fillRect(35, 64, 35, h, EYE_COLOR); canvas.fillRect(90, 64, 35, h, EYE_COLOR);
     if (frameCount % 60 < 40) { canvas.setCursor(80 + sin(frameCount*0.1)*10, 44 - (frameCount%60)); canvas.setTextColor(EYE_COLOR); canvas.print("z"); }
  }
  frameCount++;
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 160, 128);
  drawPageIndicators();
}

// ==========================================
// CORE SYSTEM
// ==========================================

void setup() {
  Serial.begin(115200);
  
  GPS_Serial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN); 

  pinMode(DISPLAY_PWR, OUTPUT);
  digitalWrite(DISPLAY_PWR, HIGH);
  pinMode(NAV_BTN, INPUT_PULLUP);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(VIBE_PIN, OUTPUT);

  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);

  SPI.begin(48, 47, 38, -1); 
  delay(50);

  tft.initR(INITR_GREENTAB); 
  tft.setRotation(3); 
  tft.fillScreen(C_BLACK);

  setupGyro();
  setupBLE(); 
  
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21); // MAX VOLUME FIXED
}

void loop() {
  audio.loop(); 
  
  // --- FEED THE GPS CONSTANTLY ---
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }
  
  // Update Emergency Screen if GPS changes
  if (currentPage == EMERGENCY && gps.location.isUpdated()) {
     forceUIRefresh = true;
  }

  // Animate the fake EQ on the Music Page
  if (currentPage == MUSIC && musicPlaying && millis() - lastEqUpdate > 150) {
     lastEqUpdate = millis();
     forceUIRefresh = true;
  }

  handleInputs();
  
  if (!musicPlaying) {
    handleLogic();
  }

  if (currentPage != lastPage || forceUIRefresh) {
    if (currentPage != lastPage) tft.fillScreen(C_BLACK); 
    refreshUI();
    lastPage = currentPage;
    forceUIRefresh = false;
  }
}

void handleInputs() {
  if (currentPage == PAIRING) return;

  static bool prevNav = false;
  bool currentNav = (digitalRead(NAV_BTN) == LOW);
  
  if (currentNav && !prevNav) {
    if(musicPlaying) { audio.stopSong(); musicPlaying = false; sysLog = "Ready."; }
    currentPage = (OS_State)((currentPage + 1) % 5);
    if (currentPage == PAIRING) currentPage = HOME; 
    vibe(30);
    delay(100);
  }
  prevNav = currentNav;

  if (digitalRead(TOUCH_PIN) == HIGH) {
    if (touchStartTime == 0) touchStartTime = millis();
  } else {
    if (touchStartTime != 0) {
      unsigned long dur = millis() - touchStartTime;
      
      // LONG PRESS (800ms)
      if (dur > 800) { 
        if (currentPage == FOCUS) {
          focusSecondsLeft = 25 * 60; focusActive = false;
        } 
        else if (currentPage == EMERGENCY) {
          // SEND SOS!
          if (gps.location.isValid()) {
             sendWhatsAppSOS(gps.location.lat(), gps.location.lng());
          } else {
             tft.fillRect(15, 80, 130, 20, C_CREAM);
             tft.setCursor(15, 85); tft.setTextColor(C_RED); tft.print("NO GPS LOCK!");
             vibe(300);
             delay(1500);
          }
        }
        vibe(150);
      } 
      
      // SHORT TAP
      else { 
        if (currentPage == FOCUS) focusActive = !focusActive;
        else if (currentPage == MUSIC) {
          if (!musicPlaying && WiFi.status() == WL_CONNECTED) { 
            sysLog = "Buffering..."; forceUIRefresh = true; refreshUI();
            audio.connecttohost(liveStreamURL);
            musicPlaying = true; 
            sysLog = "LIVE RADIO";
          } else if (musicPlaying) {
            audio.stopSong(); 
            musicPlaying = false;
            sysLog = "Ready.";
          } else {
            sysLog = "NO WIFI";
          }
        }
        vibe(50);
      }
      forceUIRefresh = true; 
      touchStartTime = 0;
    }
  }
}

void handleLogic() {
  if (currentPage == PAIRING && bleDataReceived) {
      tft.fillScreen(C_BLACK);
      tft.setCursor(10, 50); tft.setTextColor(C_MINT); tft.setTextSize(1);
      tft.print("Connecting Wi-Fi:");
      tft.setCursor(10, 70); tft.setTextColor(C_WHITE);
      tft.print(receivedSSID);

      WiFi.begin(receivedSSID.c_str(), receivedPASS.c_str());
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
          delay(500);
          tft.print(".");
          attempts++;
      }

      tft.fillScreen(C_BLACK);
      if (WiFi.status() == WL_CONNECTED) {
          // MAGIC BULLET FOR AUDIO STUTTERING: Force Wi-Fi to stay awake!    
          tft.setCursor(20, 60); tft.setTextColor(C_MINT); tft.setTextSize(2);
          tft.print("CONNECTED!");
          vibe(500);
          delay(1500); 

          NimBLEDevice::deinit(true);
          currentPage = HOME;
      } else {
          tft.setCursor(20, 60); tft.setTextColor(C_RED); tft.setTextSize(2);
          tft.print("WIFI FAILED.");
          delay(2000);
          bleDataReceived = false; 
      }
      forceUIRefresh = true;
      return;
  }

  if (currentPage == HOME) {
     runBeanLogic();
  }

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
    case PAIRING:   drawPairing(); break;
    case HOME:      break;
    case EMERGENCY: drawEmergency(); break;
    case FOCUS:     drawFocus(true); break;
    case MUSIC:     drawMusic(); break;
    case BREATHE:   tft.fillScreen(C_BLACK); break;
  }
  if (currentPage != HOME && currentPage != PAIRING) drawPageIndicators();
}

void drawPairing() {
  tft.fillRoundRect(10, 10, 140, 108, 8, C_GREY);
  tft.setTextColor(C_MINT); tft.setTextSize(1);
  tft.setCursor(25, 25); tft.print("WAITING FOR APP");
  
  tft.setTextColor(C_WHITE);
  tft.setCursor(20, 50); tft.print("BLE: BEAN_ROBOT");
  
  tft.setTextColor(C_CREAM);
  tft.setCursor(20, 80); tft.print("PIN Code: ");
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  tft.setCursor(85, 73); tft.print("1234");
}

void drawMusic() {
  tft.fillRoundRect(10, 10, 140, 95, 15, C_GREY);
  
  tft.setTextColor(C_MINT); tft.setTextSize(1);
  tft.setCursor(20, 20); tft.print("INTERNET RADIO");
  tft.drawFastHLine(20, 32, 120, C_CREAM);

  tft.setTextColor(C_WHITE); 
  tft.setCursor(20, 45); tft.print("Station: SomaFM");
  
  tft.setCursor(20, 60); 
  tft.setTextColor(musicPlaying ? C_MINT : C_CREAM);
  tft.print(sysLog);

  // Cool Retro Equalizer Animation
  if (musicPlaying) {
    for (int i = 0; i < 9; i++) {
      int barHeight = random(5, 25);
      // Erase old bar, draw new bar
      tft.fillRect(25 + (i * 12), 70, 8, 25, C_GREY);
      tft.fillRect(25 + (i * 12), 95 - barHeight, 8, barHeight, C_MINT);
    }
  } else {
    // Draw flat EQ when stopped
    for (int i = 0; i < 9; i++) {
      tft.fillRect(25 + (i * 12), 70, 8, 25, C_GREY);
      tft.fillRect(25 + (i * 12), 90, 8, 5, C_CREAM); 
    }
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
  tft.setTextColor(C_MINT); tft.setTextSize(1);
  tft.setCursor(55, 110); tft.print(msg);
}

void drawEmergency() {
  tft.fillRoundRect(5, 5, 150, 105, 12, C_CREAM);
  tft.setTextColor(C_BLACK); tft.setTextSize(1);
  tft.setCursor(15, 18); tft.print("EMERGENCY CARE");
  tft.drawFastHLine(15, 30, 130, C_GREY);
  tft.setCursor(15, 40); tft.print("Contact: Mom");
  
  tft.setCursor(15, 55); 
  if (gps.location.isValid()) {
     tft.print("Lat: "); tft.print(gps.location.lat(), 4);
     tft.setCursor(15, 67);
     tft.print("Lng: "); tft.print(gps.location.lng(), 4);
  } else {
     tft.print("GPS: Searching...");
  }

  tft.setCursor(15, 85);
  tft.setTextColor(C_GREY);
  tft.print("Hold sensor to SOS");
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
  for (int i = 0; i < 4; i++) {
    tft.fillCircle(62 + (i * 12), 122, (i == (currentPage-1) ? 3 : 1), C_WHITE);
  }
}

void vibe(int ms) {
  digitalWrite(VIBE_PIN, HIGH);
  delay(ms);
  digitalWrite(VIBE_PIN, LOW);
}

// Audio Engine Callbacks
void audio_eof_mp3(const char *info){  
    sysLog = "Stream Ended";
    musicPlaying = false;
    forceUIRefresh = true;
}
void audio_info(const char *info){
    // Kept quiet to avoid console spam!
}