// Bean Robot - Secrets Configuration Template
// INSTRUCTIONS: Rename this file to 'secrets.h' and enter your actual credentials.
// WARNING: Ensure 'secrets.h' is added to your .gitignore before committing!

#ifndef SECRETS_H
#define SECRETS_H

// --- Wi-Fi Credentials ---
const char* SECRET_SSID = "YOUR_WIFI_SSID_HERE";
const char* SECRET_PASS = "YOUR_WIFI_PASSWORD_HERE";

// --- API Keys & External Services ---
const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY_HERE";
const char* WHATSAPP_API_KEY = "YOUR_CALLMEBOT_API_KEY_HERE";

// --- Local Server Configuration ---
// Update this to match the IPv4 address of the machine running server.py
const char* LOCAL_AI_SERVER = "http://192.168.1.XXX:5000";

#endif // SECRETS_H