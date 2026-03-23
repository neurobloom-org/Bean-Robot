// Bean Robot - Hardware Diagnostic & Unit Test
// Run this standalone sketch to verify I2C and GPIO wiring before flashing main firmware.

#include <Wire.h>

#define TOUCH_PIN 14
#define VIBE_PIN 8
#define MPU_ADDR 0x68

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- 🤖 BEAN HARDWARE DIAGNOSTICS ---");

  // 1. Test I2C Bus (MPU6050 Gyroscope)
  Serial.print("Testing I2C Bus... ");
  Wire.begin(11, 12); // SDA, SCL for Nano ESP32
  Wire.beginTransmission(MPU_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("[PASS] MPU6050 Detected.");
  } else {
    Serial.println("[FAIL] MPU6050 Not Found! Check D11/D12 wiring.");
  }

  // 2. Setup GPIO
  pinMode(TOUCH_PIN, INPUT);
  pinMode(VIBE_PIN, OUTPUT);
  
  Serial.println("--- DIAGNOSTICS READY ---");
  Serial.println("Touch the sensor to test input and haptic motor.");
}

void loop() {
  int touchState = digitalRead(TOUCH_PIN);
  
  if (touchState == HIGH) {
    Serial.println("[EVENT] Touch Detected! Firing haptics...");
    digitalWrite(VIBE_PIN, HIGH);
    delay(200);
    digitalWrite(VIBE_PIN, LOW);
    delay(500); // Debounce
  }
  
  delay(50);
}