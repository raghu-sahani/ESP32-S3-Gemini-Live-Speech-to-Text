# Firmware

Paste the final tested Arduino sketch into:

```text
ESP32_S3_Gemini_Live_STT.ino
```

The file is intentionally provided as a placeholder so the working code can be uploaded separately.

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
