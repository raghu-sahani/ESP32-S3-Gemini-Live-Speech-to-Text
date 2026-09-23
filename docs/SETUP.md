# Setup and Build Guide

## 1. Install Arduino IDE

Install a current Arduino IDE version and add the ESP32 board package through Boards Manager.

## 2. Select the ESP32-S3 board

Typical starting settings:

```text
Board: ESP32S3 Dev Module
USB CDC On Boot: Enabled
CPU Frequency: 240 MHz
Flash Mode: QIO
Upload Speed: 460800 or 921600
```

Set Flash Size and PSRAM according to the exact ESP32-S3 board you own.

## 3. Install libraries

Arduino IDE -> Sketch -> Include Library -> Manage Libraries.

Install:

```text
U8g2
ArduinoJson
```

The ESP32 Arduino core provides the remaining dependencies.

## 4. Test the OLED first

Use this simple test before combining everything:

```cpp
#include <Wire.h>
#include <U8g2lib.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);

void setup() {
  Wire.begin(6, 5);
  u8g2.begin();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(15, 25, "OLED OK");
  u8g2.sendBuffer();
}

void loop() {}
```

If `OLED OK` appears correctly, continue.

## 5. Wire the microphone

Use:

```text
VDD -> 3.3V
GND -> GND
SCK -> GPIO 7
WS  -> GPIO 8
SD  -> GPIO 9
L/R -> GND
```

The project captures 32-bit I2S words from the microphone and converts them to signed 16-bit PCM for Gemini.

## 6. Wire the push button

```text
GPIO 4 ---- push button ---- GND
```

The firmware should use `INPUT_PULLUP`.

## 7. Create a Gemini API key

Create a Gemini API key in Google AI Studio.

Never paste a real key into a public GitHub commit.

Keep local settings in the sketch as:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY";
```

Replace these values only on your local machine.

## 8. Open the firmware

The working sketch is already included at:

```text
firmware/ESP32_S3_Gemini_Live_STT.ino
```

Open it in Arduino IDE and add your Wi-Fi credentials and Gemini API key locally before compiling.

## 9. Compile

Verify the sketch in Arduino IDE.

If compilation fails, check:

- Correct ESP32 board package installed
- U8g2 installed
- ArduinoJson installed
- ESP32-S3 selected as the board

## 10. Upload

Connect the ESP32-S3 over USB.

Select the correct COM port and upload.

If the board enters an upload loop or disappears:

1. Hold BOOT.
2. Press RESET.
3. Release RESET.
4. Release BOOT.
5. Upload again.
6. Re-check the COM port after upload.

## 11. Serial Monitor

Open Serial Monitor:

```text
115200 baud
```

A healthy startup should progress through Wi-Fi, WebSocket handshake, Gemini setup, and finally `READY`.

## 12. Test speech-to-text

1. Wait for `READY`.
2. Hold the push button.
3. Speak.
4. Release.
5. Wait for the final transcript on the OLED.

For best results:

- Speak 10-30 cm from the microphone.
- Avoid touching the microphone PCB while speaking.
- Use a stable Wi-Fi connection.
- Avoid clipping by using moderate microphone gain.

## 13. Expected data path

```text
INMP441 / I2S Mic
        |
        v
ESP32-S3 I2S RX
        |
        v
32-bit microphone sample
        |
        v
16-bit signed PCM
        |
        v
Base64 encoded small audio chunk
        |
        v
Gemini Live secure WebSocket
        |
        v
Transcription JSON
        |
        v
ESP32 text parser
        |
        v
SH1106 OLED
```

