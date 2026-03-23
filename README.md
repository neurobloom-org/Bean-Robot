# 🤖 Bean: Empathetic AI Companion Robot

This repository contains the complete hardware firmware and local cloud-bridge server for **Bean**, a physical, AI-driven companion robot. Engineered around the **Arduino Nano ESP32 (ESP32-S3)**, Bean features real-time voice conversations, dynamic emotional responses, guided breathing exercises, and emergency SOS capabilities.

## 🌟 Key Features

* **Real-Time AI Voice Assistant:** Records user audio via an I2S microphone, processes it through a local Python Flask server bridging to **Google Gemini Flash**, and streams natural voice responses back via Edge-TTS.
* **Hardware-Isolated Audio Architecture:** To prevent Watchdog crashes (TG1WDT_SYS_RST) and memory collisions, the input (INMP441) and output (MAX98357A) peripherals are explicitly isolated onto separate I2S hardware channels (Channel 1 and Channel 0).
* **Dynamic Emotion Engine:** An MPU-6050 gyroscope continuously monitors device handling. Bean's digital eyes (rendered on a 1.8" TFT LCD) organically shift between emotions (Happy, Scared, Confused, Sleepy) based on physical stimuli.
* **BLE Provisioning & Security:** Uses Bluetooth Low Energy (BLE) with a 4-digit PIN verification system to securely transfer and connect to local Wi-Fi networks without hardcoding credentials into the deployment build.
* **Emergency SOS Protocol:** A long-press trigger utilizes the ESP32's native Wi-Fi stack to dispatch silent GPS/Location alerts via the CallMeBot (WhatsApp) API.
* **Haptic Feedback:** A coin-cell vibration motor provides tactile confirmation for UI navigation and pacing for anxiety-grounding breathing exercises.

## 🧰 Hardware Stack & Pinout Structure

Bean is built on the **Arduino Nano ESP32**. Due to the strict memory mapping of the ESP32-S3 chip, pins have been carefully assigned to prevent internal PSRAM/Flash collisions.

| Component | Function | Nano ESP32 Pin (Hardware GPIO) |
| :--- | :--- | :--- |
| **INMP441 (Mic)** | I2S Data (SD) | `RX0` (GPIO 44) |
| | I2S Clock (SCK) | `A1` (GPIO 2) |
| | I2S Word Select (WS) | `A0` (GPIO 1) |
| **MAX98357A (Amp)**| I2S Data (DIN) | `D2` (GPIO 5) |
| | I2S Clock (BCLK)| `D3` (GPIO 6) |
| | I2S Word Select | `D4` (GPIO 7) |
| | Amplifier Wake | Wired directly to `3.3V` / `5V` |
| **MPU-6050** | I2C SDA | `D11` (GPIO 11) |
| | I2C SCL | `D12` (GPIO 12) |
| **TFT Display** | SPI CS, DC, RST | `21`, `17`, `10` |
| **UI Controls** | Capacitive Touch| `A7` (GPIO 14) |
| | Nav Button | `D4` (GPIO 4) |
| | Vibe Motor | `D8` (GPIO 8) |

## 📂 Repository Structure

* `/Arduino` — Contains the primary C++ firmware for the ESP32-S3. Manages the state machine (Pairing, Home, AI Chat, Radio, Focus, Breathe, SOS), I2S audio drivers, and the Adafruit GFX rendering pipeline.
* `/Server` — Contains `server.py`, a lightweight Flask backend. It receives raw 16kHz PCM audio buffers from the ESP32, applies WAV headers, queries the Gemini API, and returns an MP3 audio stream to the robot.

## 🚀 Getting Started

### 1. The AI Backend (Python)
1. Navigate to the `/Server` directory.
2. Install dependencies: `pip install -r requirements.txt`
3. Add your Gemini API key to `os.environ["GOOGLE_API_KEY"]` inside `server.py`.
4. Run the server: `python server.py`. Note the local IPv4 address of your machine.

### 2. The Robot Firmware (C++)
1. Open the `.ino` file in the Arduino IDE.
2. Select **Arduino Nano ESP32** as your target board.
3. In the code, update `serverIp` to match the IPv4 address of the machine running your Python server.
4. Ensure the following libraries are installed: `ESP32-audioI2S`, `Adafruit GFX`, `Adafruit ST7735`, `NimBLEDevice`.
5. Compile and upload. Open the Serial Monitor at `115200` baud to view the boot diagnostic logs.

## 🧠 Development Notes
A major milestone in this project was bypassing the default Arduino IDE pin-translation layers, which were causing instant Watchdog Timer resets when initializing the audio peripherals. The final firmware utilizes raw ESP32-S3 GPIO integers to guarantee absolute hardware stability.
