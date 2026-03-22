#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Wire.h> 
#include <driver/i2s.h>
#include <math.h>
#include "logo.h" // <--- MUST MATCH YOUR TAB NAME

// =======================
//   PIN DEFINITIONS
// =======================
#define TFT_CS     5
#define TFT_RST    4
#define TFT_DC     13 
#define TFT_MOSI   23
#define TFT_SCLK   18

#define SPK_BCLK   26
#define SPK_LRC    25
#define SPK_DIN    33 

#define GYRO_SDA   21
#define GYRO_SCL   22
#define MPU_ADDR   0x68

#define TOUCH_PIN  34
#define VIBE_PIN   2   

// =======================
//   COLORS & THEME
// =======================
#define C_BLACK      0x0000 
#define C_WHITE      0xFFFF 
#define C_GREY       0x2124 
#define C_RED        0x001F 
#define C_GREEN      0x07E0 
#define C_BLUE       0xF800
#define C_YELLOW     0x07FF 
#define C_CYAN       0xFFE0 
#define C_PALE_GREEN 0x87F0 

// Theme Globals
uint16_t UI_COLOR = C_PALE_GREEN; 
uint16_t EYE_COLOR = C_CYAN;      

// =======================
//   SYSTEM GLOBALS
// =======================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(160, 128); 

// System State
enum SystemState { MENU, APP_BEAN, APP_GAME, APP_MEDITATE, APP_THEMES, APP_SETTINGS };
SystemState currentState = MENU;

// Input Variables
bool buttonActive = false;
unsigned long pressTime = 0;
int16_t AcX, AcY, AcZ; 

// Menu Variables
const char* menuItems[] = { "BEAN INTERFACE", "SPACE DODGER", "MEDITATION", "THEMES", "SYSTEM INFO" };
int menuLength = 5;
int selectedItem = 0;
unsigned long lastScrollTime = 0;

// =======================
//   AUDIO ENGINE
// =======================
#define SAMPLE_RATE 16000

void stopSound() {
  size_t bytes_written;
  int samples = (SAMPLE_RATE * 50) / 1000; 
  int16_t *buffer = (int16_t *)calloc(samples * 2, sizeof(int16_t)); 
  if (!buffer) return;
  i2s_write(I2S_NUM_1, buffer, samples * 2, &bytes_written, portMAX_DELAY);
  i2s_zero_dma_buffer(I2S_NUM_1);
  free(buffer);
}

void playTone(int freq, int durationMs, int volume) {
  size_t bytes_written;
  int samples = (SAMPLE_RATE * durationMs) / 1000;
  int16_t *buffer = (int16_t *)malloc(samples * 2);
  if (!buffer) return;
  for (int i = 0; i < samples; i++) {
    int16_t amplitude = volume; 
    int16_t sample = ((i * freq * 2 / SAMPLE_RATE) % 2) ? amplitude : -amplitude;
    buffer[i] = sample;
  }
  i2s_write(I2S_NUM_1, buffer, samples * 2, &bytes_written, portMAX_DELAY);
  free(buffer);
}

void playSlide(int startFreq, int endFreq, int duration, int volume) {
  int steps = 15;
  int stepTime = duration / steps;
  int freqStep = (endFreq - startFreq) / steps;
  for(int i=0; i<steps; i++) {
    size_t bytes_written;
    int samples = (SAMPLE_RATE * stepTime) / 1000;
    int16_t *buffer = (int16_t *)malloc(samples * 2);
    if(buffer) {
        int freq = startFreq + (i * freqStep);
        for (int k = 0; k < samples; k++) {
           int16_t amp = volume;
           buffer[k] = ((k * freq * 2 / SAMPLE_RATE) % 2) ? amp : -amp;
        }
        i2s_write(I2S_NUM_1, buffer, samples * 2, &bytes_written, portMAX_DELAY);
        free(buffer);
    }
  }
  stopSound();
}

// --- SOUND FX ---
void sfxMenuScroll() { playTone(2000, 20, 8000); stopSound(); }
void sfxMenuSelect() { playTone(1500, 80, 10000); stopSound(); }
void sfxBack() { playTone(800, 100, 8000); playTone(600, 100, 8000); stopSound(); }
void sfxGamePoint() { playTone(3000, 50, 10000); stopSound(); }
void sfxGameCrash() { playTone(200, 300, 15000); stopSound(); }

// Bean Sounds
void sfxStartup() { playTone(500, 80, 8000); playTone(1000, 80, 8000); playTone(1500, 80, 8000); playTone(2000, 200, 10000); stopSound(); }
void sfxBlink() { playTone(800, 50, 9000); playTone(1200, 50, 9000); playTone(1600, 100, 10000); stopSound(); }
void sfxLove() { playTone(1046, 80, 10000); playTone(1318, 80, 10000); playTone(1568, 80, 10000); playTone(2093, 150, 10000); stopSound(); }
void sfxConfused() { playSlide(500, 1500, 300, 12000); }
void sfxScared() { for(int i=0; i<5; i++) { playTone(2000, 30, 15000); playTone(1500, 30, 15000); } stopSound(); }

// =======================
//   GYRO HELPER
// =======================
void setupGyro() {
  Wire.begin(GYRO_SDA, GYRO_SCL);
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x6B); Wire.write(0); Wire.endTransmission(true);
}

void readGyroRaw() {
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  if (Wire.available() >= 6) {
    AcX = (Wire.read() << 8 | Wire.read());
    AcY = (Wire.read() << 8 | Wire.read());
    AcZ = (Wire.read() << 8 | Wire.read());
  }
}

// =======================
//   APP 1: BEAN INTERFACE
// =======================
enum Emotion { NORMAL, HAPPY, ANGRY, CONFUSED, SCARED, SLEEPY };
Emotion currentEmotion = NORMAL;
unsigned long lastInteraction = 0;
int frameCount = 0;
long nextBlink = 0; 
bool isBlinking = false; int blinkHeight = 55;
int gyroOffsetX = 0, gyroOffsetY = 0;

void drawPixelHeart(int x, int y, int size, uint16_t color) {
  int s = size / 2; canvas.fillRoundRect(x-s, y-s, s, s, 4, color); canvas.fillRoundRect(x, y-s, s, s, 4, color);
  canvas.fillRect(x-(s/2), y-(s/2), s, s, color); canvas.fillTriangle(x-s, y-(s/4), x+s, y-(s/4), x, y+s+2, color);
}
void drawSwirl(int x, int y, int t) {
  float prevX=-999, prevY=-999;
  for (float i=0; i<40; i+=2.0) { 
      float angle = (i * 0.3) + (t * 0.2); float r = i / 1.5;
      int currX = x + (cos(angle)*r); int currY = y + (sin(angle)*r);
      if (prevX!=-999) canvas.drawLine(prevX, prevY, currX, currY, EYE_COLOR);
      prevX = currX; prevY = currY;
  }
}

void initBean() {
  currentEmotion = NORMAL;
  lastInteraction = millis();
  sfxStartup();
}

void runBeanLogic() {
  unsigned long now = millis();
  readGyroRaw();
  
  // Gyro Calc
  int16_t safeX = constrain(AcX, -4000, 4000);
  int16_t safeY = constrain(AcY, -4000, 4000);
  int targetX = map(safeY, -4000, 4000, -30, 30);
  int targetY = map(safeX, -4000, 4000, -20, 20);
  gyroOffsetX = (gyroOffsetX * 0.7) + (targetX * 0.3);
  gyroOffsetY = (gyroOffsetY * 0.7) + (targetY * 0.3);

  long totalForce = abs(AcX) + abs(AcY) + abs(AcZ);
  bool touched = digitalRead(TOUCH_PIN);
  
  if (touched) {
     if (currentEmotion != HAPPY) { currentEmotion = HAPPY; sfxLove(); digitalWrite(VIBE_PIN, HIGH); delay(100); digitalWrite(VIBE_PIN, LOW); }
     lastInteraction = now;
  } else if (totalForce > 50000 && currentEmotion != SLEEPY) {
     lastInteraction = now;
     if (random(0,2)==0) { currentEmotion = CONFUSED; sfxConfused(); } else { currentEmotion = SCARED; sfxScared(); }
     digitalWrite(VIBE_PIN, HIGH); delay(200); digitalWrite(VIBE_PIN, LOW); delay(500);
  } else if ((currentEmotion == SCARED || currentEmotion == CONFUSED || currentEmotion == HAPPY) && (now - lastInteraction > 5000)) {
     currentEmotion = NORMAL;
  } else if (currentEmotion != SLEEPY && (now - lastInteraction > 60000)) {
     currentEmotion = SLEEPY;
  }

  canvas.fillScreen(C_BLACK);
  
  if (currentEmotion == NORMAL) {
     if (now > nextBlink && !isBlinking) { isBlinking = true; nextBlink = now + random(2000, 5000); sfxBlink(); }
     if (isBlinking) { blinkHeight -= 8; if (blinkHeight <= 4) { blinkHeight = 4; isBlinking = false; } } else if (blinkHeight < 55) blinkHeight += 8;
     canvas.fillRoundRect(35+gyroOffsetX, 64+gyroOffsetY-(blinkHeight/2), 35, blinkHeight, 14, EYE_COLOR);
     canvas.fillRoundRect(90+gyroOffsetX, 64+gyroOffsetY-(blinkHeight/2), 35, blinkHeight, 14, EYE_COLOR);
  } 
  else if (currentEmotion == HAPPY) {
     float pulse = (sin(frameCount * 0.2) * 5); 
     drawPixelHeart(52, 64, 35+pulse, C_RED); drawPixelHeart(108, 64, 35+pulse, C_RED);
  }
  else if (currentEmotion == SCARED) { 
     // Violent Shiver + Angled Eyes
     int sx = random(-4, 5); int sy = random(-4, 5);
     int cx = 80 + sx; int cy = 64 + sy;
     canvas.fillRoundRect(cx-45, cy-25, 35, 55, 8, EYE_COLOR); canvas.fillTriangle(cx-45, cy-25, cx-10, cy-25, cx-45, cy+10, C_BLACK);
     canvas.fillRoundRect(cx+10, cy-25, 35, 55, 8, EYE_COLOR); canvas.fillTriangle(cx+45, cy-25, cx+10, cy-25, cx+45, cy+10, C_BLACK);
  }
  else if (currentEmotion == CONFUSED) {
     drawSwirl(40, 64, frameCount); drawSwirl(120, 64, frameCount);
  }
  else if (currentEmotion == SLEEPY) {
     int h = 4 + (sin(frameCount * 0.08) * 2);
     canvas.fillRect(35, 64, 35, h, EYE_COLOR); canvas.fillRect(90, 64, 35, h, EYE_COLOR);
     if (frameCount % 60 < 40) { canvas.setCursor(80 + sin(frameCount*0.1)*10, 44 - (frameCount%60)); canvas.setTextColor(EYE_COLOR); canvas.print("z"); }
  }
  frameCount++;
}

// =======================
//   APP 2: SPACE DODGER
// =======================
int playerY = 64;
int obstacleX = 160; int obstacleY = 60;
int score = 0; bool gameOver = false;

void initGame() { score = 0; gameOver = false; obstacleX = 160; playerY = 64; }

void runGameLogic() {
  canvas.fillScreen(C_BLACK);
  if (gameOver) {
    canvas.setCursor(40, 50); canvas.setTextColor(C_RED); canvas.setTextSize(2); canvas.print("CRASH!");
    canvas.setCursor(45, 75); canvas.setTextSize(1); canvas.setTextColor(C_WHITE); canvas.print("Score: "); canvas.print(score);
    if (digitalRead(TOUCH_PIN)) { initGame(); delay(500); }
    return;
  }
  readGyroRaw();
  int moveY = map(constrain(AcX, -6000, 6000), -6000, 6000, 6, -6);
  playerY += moveY; playerY = constrain(playerY, 8, 120);
  obstacleX -= 6; 
  if (obstacleX < 0) { obstacleX = 160; obstacleY = random(20, 108); score++; sfxGamePoint(); }
  if (obstacleX < 20 && obstacleX > 5 && abs(playerY - obstacleY) < 18) { gameOver = true; sfxGameCrash(); digitalWrite(VIBE_PIN, HIGH); delay(200); digitalWrite(VIBE_PIN, LOW); }
  canvas.fillCircle(15, playerY, 6, EYE_COLOR); 
  canvas.fillRect(obstacleX, obstacleY-12, 12, 24, C_RED); 
  canvas.setCursor(140, 5); canvas.setTextColor(C_WHITE); canvas.setTextSize(1); canvas.print(score);
  delay(30); 
}

// =======================
//   APP 3: MEDITATION
// =======================
bool isMeditating = false;
float breathVal = 0;

void runMeditation() {
  canvas.fillScreen(C_BLACK);
  
  if (!isMeditating) {
    canvas.setCursor(30, 50); canvas.setTextColor(UI_COLOR); canvas.setTextSize(1); canvas.print("Tap to Start");
    canvas.setCursor(45, 70); canvas.print("BREATHING");
    if (digitalRead(TOUCH_PIN)) { isMeditating = true; breathVal = 4.71; sfxMenuSelect(); delay(500); }
  } else {
    breathVal += 0.08; 
    int r = 30 + (sin(breathVal) * 15); 
    
    // Instant Text Switch
    canvas.setCursor(55, 110); 
    if (sin(breathVal - 0.5) > 0) { 
       canvas.setTextColor(UI_COLOR); canvas.print("INHALE");
    } else {
       canvas.setTextColor(C_WHITE); canvas.print("EXHALE");
    }
    
    canvas.fillCircle(80, 64, r, UI_COLOR);
    canvas.drawCircle(80, 64, r+2, C_WHITE);
    
    if (digitalRead(TOUCH_PIN)) { isMeditating = false; delay(500); }
    delay(20); 
  }
}

// =======================
//   APP 4: THEMES
// =======================
int themeSelection = 0; 
const char* colorsNames[] = { "RED", "YELLOW", "GREEN", "BLUE", "CYAN", "WHITE" };
uint16_t colorsHex[] = { C_RED, C_YELLOW, C_GREEN, C_BLUE, C_CYAN, C_WHITE };
int eyeColorIdx = 4; // Cyan Default
int uiColorIdx = 2; // Green Default

void runThemes() {
  canvas.fillScreen(C_BLACK);
  canvas.setCursor(45, 10); canvas.setTextColor(C_WHITE); canvas.print("THEME EDITOR");
  
  // Selectors
  canvas.drawRect(10, 30, 140, 25, (themeSelection==0)? C_WHITE : C_GREY);
  canvas.setCursor(20, 38); canvas.setTextColor(C_WHITE); canvas.print("EYES: ");
  canvas.setTextColor(colorsHex[eyeColorIdx]); canvas.print(colorsNames[eyeColorIdx]);
  
  canvas.drawRect(10, 60, 140, 25, (themeSelection==1)? C_WHITE : C_GREY);
  canvas.setCursor(20, 68); canvas.setTextColor(C_WHITE); canvas.print("UI:   ");
  canvas.setTextColor(colorsHex[uiColorIdx]); canvas.print(colorsNames[uiColorIdx]);

  // Back Button
  canvas.drawRect(10, 90, 140, 25, (themeSelection==2)? C_WHITE : C_GREY);
  canvas.setCursor(65, 98); canvas.setTextColor(C_WHITE); canvas.print("BACK");
  
  if (digitalRead(TOUCH_PIN)) {
    if (themeSelection == 0) { 
      eyeColorIdx = (eyeColorIdx + 1) % 6; EYE_COLOR = colorsHex[eyeColorIdx];
    } else if (themeSelection == 1) {
      uiColorIdx = (uiColorIdx + 1) % 6; UI_COLOR = colorsHex[uiColorIdx];
    } else {
      currentState = MENU; 
    }
    sfxMenuSelect();
    delay(300);
  }
  
  readGyroRaw();
  if (AcX < -4000) { themeSelection++; if(themeSelection>2) themeSelection=0; delay(200); }
  if (AcX > 4000) { themeSelection--; if(themeSelection<0) themeSelection=2; delay(200); }
}

// =======================
//   APP 5: SYSTEM INFO
// =======================
void runSettings() {
  canvas.fillScreen(C_BLACK);
  canvas.fillRoundRect(10, 10, 140, 108, 5, C_GREY);
  canvas.setCursor(25, 25); canvas.setTextColor(UI_COLOR); canvas.setTextSize(2); canvas.print("SYSTEM");
  canvas.setCursor(20, 55); canvas.setTextColor(C_WHITE); canvas.setTextSize(1); canvas.print("Battery: 100%");
  canvas.setCursor(20, 75); canvas.print("Gyro: ONLINE");
  canvas.setCursor(20, 95); canvas.print("Ver: 3.0 BLOOM");
}

// =======================
//   MENU SYSTEM (BLOOM OS)
// =======================
void runMenu() {
  canvas.fillScreen(C_BLACK);
  canvas.setCursor(55, 10); canvas.setTextColor(C_WHITE); canvas.setTextSize(1); canvas.print("BLOOM OS");
  canvas.drawLine(10, 22, 150, 22, C_GREY);

  if (millis() - lastScrollTime > 300) {
    readGyroRaw();
    if (AcX < -4000) { selectedItem = (selectedItem + 1) % menuLength; sfxMenuScroll(); lastScrollTime = millis(); }
    if (AcX > 4000) { selectedItem--; if(selectedItem < 0) selectedItem = menuLength - 1; sfxMenuScroll(); lastScrollTime = millis(); }
  }

  for (int i = 0; i < menuLength; i++) {
    int y = 35 + (i * 20); 
    if (i == selectedItem) {
      canvas.fillRoundRect(10, y, 140, 18, 4, UI_COLOR); 
      canvas.setCursor(20, y+5); canvas.setTextColor(C_BLACK);
    } else {
      canvas.drawRoundRect(10, y, 140, 18, 4, C_GREY); 
      canvas.setCursor(20, y+5); canvas.setTextColor(C_WHITE);
    }
    canvas.setTextSize(1); canvas.print(menuItems[i]);
  }
}

// =======================
//   MAIN SETUP & LOOP
// =======================
void setup() {
  Serial.begin(115200);
  pinMode(VIBE_PIN, OUTPUT); pinMode(TOUCH_PIN, INPUT);
  
  tft.initR(INITR_GREENTAB); tft.setRotation(3); tft.fillScreen(C_BLACK);

  i2s_config_t i2s_config = { .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), .sample_rate = SAMPLE_RATE, .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, .communication_format = I2S_COMM_FORMAT_I2S, .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, .dma_buf_count = 8, .dma_buf_len = 64, .use_apll = false };
  i2s_pin_config_t pin_config = { .bck_io_num = SPK_BCLK, .ws_io_num = SPK_LRC, .data_out_num = SPK_DIN, .data_in_num = -1 };
  i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL); i2s_set_pin(I2S_NUM_1, &pin_config); i2s_zero_dma_buffer(I2S_NUM_1);

  setupGyro();
  
  // === LOGO BOOT ===
  tft.fillScreen(C_BLACK);
  // Drawing 160x128 Logo (Full Screen)
  tft.drawRGBBitmap(0, 0, neuroLogo, 160, 128);
  
  delay(500);
  sfxStartup();
  delay(1000); 
  // Goes straight to Menu loop
}

void loop() {
  bool touched = digitalRead(TOUCH_PIN);
  if (touched && !buttonActive) { buttonActive = true; pressTime = millis(); }
  
  if (!touched && buttonActive) { // RELEASE
    buttonActive = false;
    unsigned long duration = millis() - pressTime;
    
    if (duration > 800) { // LONG PRESS = BACK
       if (currentState != MENU) { 
         currentState = MENU; sfxBack(); 
         stopSound(); 
       }
    } else { // SHORT TAP = SELECT
       if (currentState == APP_THEMES || currentState == APP_MEDITATE) {
          // Pass tap to app
       } else if (currentState == MENU) {
          if (selectedItem == 0) { currentState = APP_BEAN; initBean(); }
          if (selectedItem == 1) { currentState = APP_GAME; initGame(); }
          if (selectedItem == 2) { currentState = APP_MEDITATE; isMeditating = false; }
          if (selectedItem == 3) { currentState = APP_THEMES; }
          if (selectedItem == 4) { currentState = APP_SETTINGS; }
          sfxMenuSelect();
       }
    }
  }

  switch (currentState) {
    case MENU:     runMenu();      break;
    case APP_BEAN: runBeanLogic(); break;
    case APP_GAME: runGameLogic(); break;
    case APP_MEDITATE: runMeditation(); break;
    case APP_THEMES: runThemes(); break;
    case APP_SETTINGS: runSettings(); break;
  }

  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 160, 128);
}