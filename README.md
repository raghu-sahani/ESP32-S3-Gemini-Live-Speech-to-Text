# ESP32-S3 Gemini Live Speech-to-Text

A push-to-talk speech-to-text device built with an ESP32-S3, an I2S microphone, a 128x64 SH1106 OLED, and Google Gemini Live Transcription.

Hold the button, speak, release the button, and the recognized text appears on the OLED.

> The working firmware is included in `firmware/ESP32_S3_Gemini_Live_STT.ino`. Add your own Wi-Fi credentials and Gemini API key locally before uploading to the ESP32-S3.

## What it does

- Push-to-talk control with one physical button
- Captures microphone audio using I2S
- Converts microphone samples to 16-bit mono PCM at 16 kHz
- Streams audio to Gemini over a secure WebSocket connection
- Uses Gemini Live transcription for low-latency speech recognition
- Displays interim/final text on a SH1106 OLED
- Reconnects to Wi-Fi/Gemini when the connection is lost
- Handles Gemini JSON arriving in either text or binary WebSocket frames

## Hardware

- ESP32-S3 development board
- I2S microphone module such as INMP441
- 128x64 SH1106 I2C OLED
- Momentary push button
- Breadboard
- Jumper wires
- USB cable

## Pin connections

| Component | Pin | ESP32-S3 |
|---|---|---|
| SH1106 OLED | VCC | 3.3V |
| SH1106 OLED | GND | GND |
| SH1106 OLED | SDA | GPIO 6 |
| SH1106 OLED | SCL/SCK | GPIO 5 |
| I2S microphone | VDD | 3.3V |
| I2S microphone | GND | GND |
| I2S microphone | SCK/BCLK | GPIO 7 |
| I2S microphone | WS/LRCLK | GPIO 8 |
| I2S microphone | SD | GPIO 9 |
| I2S microphone | L/R | GND |
| Push button | Side 1 | GPIO 4 |
| Push button | Side 2 | GND |

The button uses the ESP32 internal pull-up, so no external pull-up resistor is required.

## System flow

```text
User holds button
       |
       v
ESP32-S3 starts microphone capture
       |
       v
16 kHz / 16-bit / mono PCM
       |
       v
Gemini Live WebSocket
       |
       v
Live transcription result
       |
       v
SH1106 OLED

Release button -> finalize utterance -> show transcript
```

## Required Arduino libraries

Install from Arduino IDE -> Library Manager:

- U8g2
- ArduinoJson

The sketch also uses libraries included with the ESP32 Arduino core:

- WiFi
- WiFiClientSecure
- Wire
- driver/i2s
- mbedtls/base64

## Arduino IDE board setup

A typical configuration is:

- Board: ESP32S3 Dev Module
- USB CDC On Boot: Enabled
- CPU Frequency: 240 MHz
- Flash Mode: QIO
- Upload Speed: 460800 or 921600
- Serial Monitor: 115200 baud

Use the Flash Size and PSRAM settings that match your exact board.

## Gemini API key

1. Open Google AI Studio.
2. Create a Gemini API key.
3. Keep the key private.
4. Put it into your local sketch only.
5. Do not commit the real API key to GitHub.

Use placeholders in public code:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY";
```

## Build

1. Wire the OLED, microphone, and push button using the table above.
2. Install the ESP32 board package in Arduino IDE.
3. Install U8g2 and ArduinoJson.
4. Open `firmware/ESP32_S3_Gemini_Live_STT.ino`.
5. Add your Wi-Fi credentials and Gemini API key locally.
6. Select the correct ESP32-S3 board and COM port.
7. Compile and upload.
8. Open Serial Monitor at 115200 baud.
9. Wait for the OLED to show `READY`.

## Use

1. Wait for `READY`.
2. Press and hold the button.
3. Speak clearly.
4. Release the button.
5. The final transcript appears on the OLED.
6. Press again for another utterance.

## OLED states

Typical states are:

```text
Speech To Text
Starting...
```

```text
READY
Hold button
and speak
```

```text
LISTENING...
Speak now
Release = finish
```

```text
FINALIZING...
Please wait
```

```text
TRANSCRIPT:
Hello everyone
```

## Important implementation detail

Gemini Live may return JSON inside a binary WebSocket frame instead of a text frame.

A correct client should therefore process both:

- opcode `0x01` - text
- opcode `0x02` - binary

This was essential for reliably detecting Gemini's `setupComplete` response on ESP32-S3.

## Documentation

- [Full setup guide](docs/SETUP.md)
- [Wiring guide](docs/WIRING.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [API-key safety](SECURITY.md)

## Security

Do not publish Wi-Fi passwords or Gemini API keys.

For a personal prototype, storing the key in local firmware is convenient. For a device that will be distributed to other people, use a backend/token service instead of embedding a permanent API key in firmware.

## Project status

Working prototype:

- OLED: working
- I2S microphone: working
- Push-to-talk button: working
- Wi-Fi: working
- Gemini Live connection: working
- Live/final speech-to-text: working
- OLED transcript display: working

