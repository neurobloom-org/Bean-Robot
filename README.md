# Bean-Robot Hardware & Firmware

This repository contains the core logic and firmware for the **Bean** physical companion. It manages the hardware-level interactions, sensor data processing, and the secure communication bridge between the robot and the mobile application.

## 📂 Repository Structure

* **/.history**: Internal development logs and previous iterations of the firmware logic.
* **/Arduino**: Contains the primary C++/Arduino sketches for the **ESP32-S3-Nano**, including motor controls, expressive logic, and BLE/WiFi drivers.
* **/Server**: Houses local communication scripts and socket logic for handling real-time requests between the robot and the cloud backend.

## 🛠️ Key Features

* **Microcontroller**: Powered by the **ESP32-S3-Nano**, chosen for its small form factor, efficiency, and dual-core processing capabilities.
* **Secure Pairing**: Implementation of a 4-digit PIN verification system via **Bluetooth Low Energy (BLE)** to ensure authorized user linking.
* **WiFi Provisioning**: A seamless bridge that receives encrypted network credentials from the mobile app to establish an internet connection.
* **Hardware Logic**: Controls the robot's physical movements and interactions based on emotional data received from the **NeuroBloom AI**.

## 🚀 Getting Started

1. **Environment**: Open the `/Arduino` folder in the Arduino IDE or VS Code (with PlatformIO).
2. **Board Selection**: Select the **ESP32-S3-Nano** as the target board.
3. **Dependencies**: Ensure the `BLEDevice`, `WiFi`, and `HTTPClient` libraries are installed.
4. **Flashing**: Connect the Bean hardware via USB and upload the main sketch to initialize the pairing mode.

## 🔒 Security
The robot uses **Non-Volatile Storage (NVS)** to securely store the hardcoded PIN and WiFi credentials. This ensures they are retained across power cycles without being exposed in the source code.
