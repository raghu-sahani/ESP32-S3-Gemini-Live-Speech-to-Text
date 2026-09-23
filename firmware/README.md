# Firmware

The tested Arduino firmware is included in:

```text
ESP32_S3_Gemini_Live_STT.ino
```

Open the sketch in Arduino IDE, add your local credentials, select the ESP32-S3 board, and upload it.

Before committing your sketch, replace real credentials with placeholders:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY";
```

Required third-party libraries:

- U8g2
- ArduinoJson

See the repository README and `docs/` folder for the complete build instructions.
