#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <Wire.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>

#include <driver/i2s.h>
#include <mbedtls/base64.h>
#include <esp_system.h>

// =====================================================
// USER SETTINGS
// =====================================================

const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

const char* GEMINI_API_KEY = "";

// =====================================================
// OLED
// =====================================================

#define OLED_SDA 6
#define OLED_SCL 5

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);

// =====================================================
// BUTTON
// =====================================================

#define BUTTON_PIN 4

// =====================================================
// MICROPHONE
// =====================================================

#define I2S_PORT I2S_NUM_0

#define I2S_SCK 7
#define I2S_WS  8
#define I2S_SD  9

#define SAMPLE_RATE 16000

#define AUDIO_SAMPLES 1024

int32_t i2sBuffer[AUDIO_SAMPLES];
int16_t pcmBuffer[AUDIO_SAMPLES];

// Settings that worked with your microphone
#define MIC_SHIFT 11
#define MIC_GAIN  3

// =====================================================
// BASE64
// =====================================================

#define PCM_BYTES \
  (AUDIO_SAMPLES * sizeof(int16_t))

#define BASE64_SIZE \
  (4 * ((PCM_BYTES + 2) / 3) + 8)

unsigned char base64Buffer[BASE64_SIZE];

// =====================================================
// GEMINI
// =====================================================

const char* GEMINI_HOST =
  "generativelanguage.googleapis.com";

WiFiClientSecure wsClient;

bool wsConnected = false;
bool geminiReady = false;

// =====================================================
// SPEECH STATE
// =====================================================

bool recording = false;
bool waitingFinal = false;

String interimTranscript = "";
String finalTranscript = "";

String audioMessage;
String fragmentedMessage;

unsigned long finalTimer = 0;

#define FINAL_TIMEOUT 5000

// =====================================================
// BUTTON
// =====================================================

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long debounceTimer = 0;

#define DEBOUNCE_TIME 25

// =====================================================
// OLED REFRESH
// =====================================================

unsigned long lastOledRefresh = 0;

#define OLED_REFRESH_TIME 150

// =====================================================
// RECONNECT
// =====================================================

unsigned long reconnectTimer = 0;

#define RECONNECT_TIME 5000

// =====================================================
// OLED
// =====================================================

void showMessage(
  String line1,
  String line2 = "",
  String line3 = ""
) {

  u8g2.clearBuffer();

  u8g2.setFont(
    u8g2_font_6x10_tf
  );

  u8g2.drawStr(
    0,
    14,
    line1.c_str()
  );

  if (line2.length()) {

    u8g2.drawStr(
      0,
      31,
      line2.c_str()
    );
  }

  if (line3.length()) {

    u8g2.drawStr(
      0,
      48,
      line3.c_str()
    );
  }

  u8g2.sendBuffer();
}

// =====================================================
// DISPLAY TRANSCRIPT
// =====================================================

void showText(
  String heading,
  String text
) {

  u8g2.clearBuffer();

  u8g2.setFont(
    u8g2_font_6x10_tf
  );

  u8g2.drawStr(
    0,
    9,
    heading.c_str()
  );

  // Keep newest section on small OLED
  if (text.length() > 95) {

    int start =
      text.length() - 95;

    while (
      start < text.length() &&
      text[start] != ' '
    ) {
      start++;
    }

    text =
      text.substring(start);

    text.trim();
  }

  const int charsPerLine = 20;

  int position = 0;
  int y = 20;

  while (
    position < text.length() &&
    y <= 60
  ) {

    int remaining =
      text.length() - position;

    int count =
      min(
        charsPerLine,
        remaining
      );

    // Try breaking at spaces
    if (
      position + count <
      text.length()
    ) {

      int spacePosition = -1;

      for (
        int i = count;
        i > 0;
        i--
      ) {

        if (
          text[position + i] ==
          ' '
        ) {

          spacePosition = i;
          break;
        }
      }

      if (spacePosition > 5) {
        count = spacePosition;
      }
    }

    String line =
      text.substring(
        position,
        position + count
      );

    line.trim();

    u8g2.drawStr(
      0,
      y,
      line.c_str()
    );

    position += count;

    while (
      position < text.length() &&
      text[position] == ' '
    ) {
      position++;
    }

    y += 10;
  }

  u8g2.sendBuffer();
}

// =====================================================
// WIFI
// =====================================================

void connectWiFi() {

  showMessage(
    "Speech To Text",
    "Connecting WiFi..."
  );

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  Serial.print(
    "Connecting WiFi"
  );

  int attempts = 0;

  while (
    WiFi.status() != WL_CONNECTED
  ) {

    delay(400);

    Serial.print(".");

    attempts++;

    if (attempts > 60) {

      showMessage(
        "WiFi ERROR",
        "Restarting..."
      );

      delay(2000);

      ESP.restart();
    }
  }

  Serial.println();
  Serial.println(
    "WiFi connected"
  );

  showMessage(
    "WiFi connected",
    "Connecting Gemini"
  );
}

// =====================================================
// MICROPHONE
// =====================================================

void setupMicrophone() {

  i2s_config_t config = {};

  config.mode =
    (i2s_mode_t)(
      I2S_MODE_MASTER |
      I2S_MODE_RX
    );

  config.sample_rate =
    SAMPLE_RATE;

  config.bits_per_sample =
    I2S_BITS_PER_SAMPLE_32BIT;

  // L/R → GND = LEFT
  config.channel_format =
    I2S_CHANNEL_FMT_ONLY_LEFT;

  config.communication_format =
    I2S_COMM_FORMAT_STAND_I2S;

  config.intr_alloc_flags =
    ESP_INTR_FLAG_LEVEL1;

  config.dma_buf_count = 8;

  config.dma_buf_len = 256;

  config.use_apll = false;

  config.tx_desc_auto_clear = false;

  config.fixed_mclk = 0;

  i2s_pin_config_t pins = {};

  pins.bck_io_num =
    I2S_SCK;

  pins.ws_io_num =
    I2S_WS;

  pins.data_out_num =
    I2S_PIN_NO_CHANGE;

  pins.data_in_num =
    I2S_SD;

  if (
    i2s_driver_install(
      I2S_PORT,
      &config,
      0,
      NULL
    ) != ESP_OK
  ) {

    showMessage(
      "MIC ERROR",
      "I2S failed"
    );

    while (true) {
      delay(1000);
    }
  }

  if (
    i2s_set_pin(
      I2S_PORT,
      &pins
    ) != ESP_OK
  ) {

    showMessage(
      "MIC ERROR",
      "Pins failed"
    );

    while (true) {
      delay(1000);
    }
  }

  i2s_zero_dma_buffer(
    I2S_PORT
  );

  Serial.println(
    "Microphone ready"
  );
}

// =====================================================
// READ EXACT NUMBER OF BYTES
// =====================================================

bool readExact(
  uint8_t* buffer,
  size_t length,
  uint32_t timeout = 2000
) {

  size_t received = 0;

  unsigned long start =
    millis();

  while (
    received < length
  ) {

    if (
      wsClient.available()
    ) {

      int count =
        wsClient.read(
          buffer + received,
          length - received
        );

      if (count > 0) {

        received += count;

        start =
          millis();
      }
    }

    else {

      if (
        !wsClient.connected()
      ) {

        return false;
      }

      if (
        millis() - start >
        timeout
      ) {

        return false;
      }

      delay(1);
    }
  }

  return true;
}

// =====================================================
// WRITE ALL
// =====================================================

bool writeAll(
  const uint8_t* data,
  size_t length
) {

  size_t sent = 0;

  while (
    sent < length
  ) {

    size_t written =
      wsClient.write(
        data + sent,
        length - sent
      );

    if (written == 0) {

      return false;
    }

    sent += written;

    delay(0);
  }

  return true;
}

// =====================================================
// WEBSOCKET CLIENT FRAME
// =====================================================

bool wsSendFrame(
  uint8_t opcode,
  const uint8_t* payload,
  size_t length
) {

  if (
    !wsClient.connected()
  ) {

    return false;
  }

  uint8_t header[14];

  int headerLength = 0;

  // FIN + opcode
  header[headerLength++] =
    0x80 | opcode;

  // Client frames MUST be masked.

  if (length < 126) {

    header[headerLength++] =
      0x80 |
      (uint8_t)length;
  }

  else if (
    length <= 65535
  ) {

    header[headerLength++] =
      0x80 | 126;

    header[headerLength++] =
      (length >> 8) & 0xFF;

    header[headerLength++] =
      length & 0xFF;
  }

  else {

    // We do not send anything this large.
    return false;
  }

  uint8_t mask[4];

  uint32_t randomValue =
    esp_random();

  mask[0] =
    randomValue & 0xFF;

  mask[1] =
    (randomValue >> 8) & 0xFF;

  mask[2] =
    (randomValue >> 16) & 0xFF;

  mask[3] =
    (randomValue >> 24) & 0xFF;

  for (
    int i = 0;
    i < 4;
    i++
  ) {

    header[headerLength++] =
      mask[i];
  }

  if (
    !writeAll(
      header,
      headerLength
    )
  ) {

    return false;
  }

  // Send masked payload in small chunks.

  uint8_t temporary[512];

  size_t position = 0;

  while (
    position < length
  ) {

    size_t count =
      min(
        (size_t)sizeof(temporary),
        length - position
      );

    for (
      size_t i = 0;
      i < count;
      i++
    ) {

      temporary[i] =
        payload[position + i] ^
        mask[
          (position + i) & 3
        ];
    }

    if (
      !writeAll(
        temporary,
        count
      )
    ) {

      return false;
    }

    position += count;
  }

  return true;
}

// =====================================================
// SEND TEXT FRAME
// =====================================================

bool wsSendText(
  const String &text
) {

  return wsSendFrame(
    0x01,
    (const uint8_t*)
      text.c_str(),
    text.length()
  );
}

// =====================================================
// SEND PONG
// =====================================================

bool wsSendPong(
  const uint8_t* data,
  size_t length
) {

  return wsSendFrame(
    0x0A,
    data,
    length
  );
}

// =====================================================
// HANDLE GEMINI JSON
// =====================================================

void handleGeminiJSON(
  const String &json
) {

  Serial.println();
  Serial.println(
    "GEMINI RX:"
  );

  Serial.println(json);

  JsonDocument doc;

  DeserializationError error =
    deserializeJson(
      doc,
      json
    );

  if (error) {

    Serial.print(
      "JSON error: "
    );

    Serial.println(
      error.c_str()
    );

    return;
  }

  // ==================================================
  // SETUP COMPLETE
  // ==================================================

  if (
    doc[
      "setupComplete"
    ].is<JsonObject>()
  ) {

    geminiReady = true;

    Serial.println(
      "GEMINI READY!"
    );

    showMessage(
      "READY",
      "Hold button",
      "and speak"
    );

    return;
  }

  // ==================================================
  // ERROR
  // ==================================================

  if (
    !doc["error"].isNull()
  ) {

    String errorText =
      doc["error"]["message"]
      | "Gemini error";

    Serial.println(
      errorText
    );

    showMessage(
      "GEMINI ERROR",
      errorText.substring(
        0,
        20
      )
    );

    return;
  }

  // ==================================================
  // SERVER CONTENT
  // ==================================================

  JsonVariant serverContent =
    doc["serverContent"];

  if (
    serverContent.isNull()
  ) {

    return;
  }

  // ==================================================
  // INTERIM
  // ==================================================

  String interim =
    serverContent[
      "interimInputTranscription"
    ][
      "text"
    ] | "";

  if (
    interim.length()
  ) {

    interim.trim();

    interimTranscript =
      interim;

    Serial.print(
      "INTERIM: "
    );

    Serial.println(
      interim
    );

    if (
      recording &&
      millis() -
      lastOledRefresh >
      OLED_REFRESH_TIME
    ) {

      showText(
        "LISTENING...",
        interim
      );

      lastOledRefresh =
        millis();
    }
  }

  // ==================================================
  // FINAL
  // ==================================================

  String finalText =
    serverContent[
      "inputTranscription"
    ][
      "text"
    ] | "";

  if (
    finalText.length()
  ) {

    finalText.trim();

    if (
      finalTranscript.length()
    ) {

      finalTranscript += " ";
    }

    finalTranscript +=
      finalText;

    finalTranscript.trim();

    waitingFinal =
      false;

    Serial.print(
      "FINAL: "
    );

    Serial.println(
      finalTranscript
    );

    showText(
      "TRANSCRIPT:",
      finalTranscript
    );
  }
}

// =====================================================
// READ ONE WEBSOCKET FRAME
// =====================================================



bool wsReadFrame() {

  if (wsClient.available() < 2) {
    return false;
  }

  uint8_t firstTwo[2];

  if (!readExact(firstTwo, 2)) {
    return false;
  }

  bool fin =
    firstTwo[0] & 0x80;

  uint8_t opcode =
    firstTwo[0] & 0x0F;

  bool masked =
    firstTwo[1] & 0x80;

  uint64_t payloadLength =
    firstTwo[1] & 0x7F;

  // ==================================================
  // EXTENDED LENGTH
  // ==================================================

  if (payloadLength == 126) {

    uint8_t extended[2];

    if (!readExact(extended, 2)) {
      return false;
    }

    payloadLength =
      ((uint16_t)extended[0] << 8) |
      extended[1];
  }

  else if (payloadLength == 127) {

    uint8_t extended[8];

    if (!readExact(extended, 8)) {
      return false;
    }

    payloadLength = 0;

    for (int i = 0; i < 8; i++) {

      payloadLength =
        (payloadLength << 8) |
        extended[i];
    }
  }

  // Protect ESP32 memory
  if (payloadLength > 20000) {

    Serial.println(
      "Incoming frame too large"
    );

    return false;
  }

  // ==================================================
  // MASK
  // ==================================================

  uint8_t mask[4] =
    {0, 0, 0, 0};

  if (masked) {

    if (!readExact(mask, 4)) {
      return false;
    }
  }

  // ==================================================
  // PAYLOAD
  // ==================================================

  uint8_t* payload =
    (uint8_t*)malloc(
      payloadLength + 1
    );

  if (!payload) {

    Serial.println(
      "WebSocket memory error"
    );

    return false;
  }

  if (payloadLength > 0) {

    if (
      !readExact(
        payload,
        payloadLength
      )
    ) {

      free(payload);
      return false;
    }
  }

  // Server frames normally aren't masked,
  // but handle it if necessary.
  if (masked) {

    for (
      size_t i = 0;
      i < payloadLength;
      i++
    ) {

      payload[i] ^=
        mask[i & 3];
    }
  }

  payload[payloadLength] = 0;

  // ==================================================
  // VERY IMPORTANT FIX
  //
  // 0x01 = TEXT
  // 0x02 = BINARY
  //
  // Gemini is sending JSON inside a binary frame.
  // ==================================================

  if (
    opcode == 0x01 ||
    opcode == 0x02
  ) {

    String jsonPart;

    jsonPart.reserve(
      payloadLength + 1
    );

    for (
      size_t i = 0;
      i < payloadLength;
      i++
    ) {

      jsonPart +=
        (char)payload[i];
    }

    Serial.println();

    if (opcode == 0x02) {
      Serial.println(
        "Gemini frame: BINARY"
      );
    }
    else {
      Serial.println(
        "Gemini frame: TEXT"
      );
    }

    if (fin) {

      Serial.println(
        "----- GEMINI JSON -----"
      );

      Serial.println(
        jsonPart
      );

      Serial.println(
        "-----------------------"
      );

      handleGeminiJSON(
        jsonPart
      );
    }

    else {

      fragmentedMessage =
        jsonPart;
    }
  }

  // ==================================================
  // CONTINUATION
  // ==================================================

  else if (
    opcode == 0x00
  ) {

    for (
      size_t i = 0;
      i < payloadLength;
      i++
    ) {

      fragmentedMessage +=
        (char)payload[i];
    }

    if (fin) {

      Serial.println(
        "----- GEMINI JSON -----"
      );

      Serial.println(
        fragmentedMessage
      );

      Serial.println(
        "-----------------------"
      );

      handleGeminiJSON(
        fragmentedMessage
      );

      fragmentedMessage = "";
    }
  }

  // ==================================================
  // PING
  // ==================================================

  else if (
    opcode == 0x09
  ) {

    wsSendPong(
      payload,
      payloadLength
    );
  }

  // ==================================================
  // CLOSE
  // ==================================================

  else if (
    opcode == 0x08
  ) {

    uint16_t closeCode = 0;

    String reason = "";

    if (payloadLength >= 2) {

      closeCode =
        ((uint16_t)payload[0] << 8) |
        payload[1];

      for (
        size_t i = 2;
        i < payloadLength;
        i++
      ) {

        reason +=
          (char)payload[i];
      }
    }

    Serial.println();
    Serial.println(
      "Gemini closed connection"
    );

    Serial.print(
      "Code: "
    );

    Serial.println(
      closeCode
    );

    Serial.print(
      "Reason: "
    );

    Serial.println(
      reason
    );

    showMessage(
      "Gemini closed",
      "Code: " +
        String(closeCode),
      reason.substring(
        0,
        20
      )
    );

    wsConnected = false;
    geminiReady = false;

    wsClient.stop();
  }

  // ==================================================
  // PONG
  // ==================================================

  else if (
    opcode == 0x0A
  ) {

    // Nothing required.
  }

  // ==================================================
  // UNKNOWN
  // ==================================================

  else {

    Serial.print(
      "Unknown WS opcode: "
    );

    Serial.println(
      opcode
    );
  }

  free(payload);

  return true;
}
void wsPoll() {

  int count = 0;

  while (
    wsClient.available() &&
    count < 10
  ) {

    wsReadFrame();

    count++;
  }

  if (
    wsConnected &&
    !wsClient.connected()
  ) {

    wsConnected = false;
    geminiReady = false;

    Serial.println(
      "Gemini connection lost"
    );
  }
}

// =====================================================
// CREATE WEBSOCKET KEY
// =====================================================

String createWebSocketKey() {

  uint8_t randomData[16];

  esp_fill_random(
    randomData,
    sizeof(randomData)
  );

  unsigned char encoded[32];

  size_t encodedLength = 0;

  mbedtls_base64_encode(
    encoded,
    sizeof(encoded) - 1,
    &encodedLength,
    randomData,
    sizeof(randomData)
  );

  encoded[
    encodedLength
  ] = 0;

  return String(
    (char*)encoded
  );
}

// =====================================================
// SEND GEMINI SETUP
// =====================================================

bool sendGeminiSetup() {

  // Google's documented manual VAD configuration.

  String setup =
    "{"
      "\"setup\":{"
        "\"model\":"
        "\"models/gemini-3.5-transcribe-live\","

        "\"generationConfig\":{"
          "\"responseModalities\":[\"TEXT\"]"
        "},"

        "\"realtimeInputConfig\":{"
          "\"automaticActivityDetection\":{"
            "\"disabled\":true"
          "}"
        "},"

        "\"inputAudioTranscription\":{}"
      "}"
    "}";

  Serial.println(
    "Sending Gemini setup..."
  );

  return wsSendText(
    setup
  );
}

// =====================================================
// CONNECT DIRECTLY TO GEMINI WEBSOCKET
// =====================================================

bool connectGemini() {

  geminiReady = false;
  wsConnected = false;

  wsClient.stop();

  showMessage(
    "Connecting Gemini",
    "Please wait..."
  );

  wsClient.setInsecure();

  wsClient.setTimeout(
    5
  );

  Serial.println(
    "Opening TLS connection..."
  );

  if (
    !wsClient.connect(
      GEMINI_HOST,
      443
    )
  ) {

    Serial.println(
      "TLS connection failed"
    );

    showMessage(
      "Gemini ERROR",
      "TLS failed"
    );

    return false;
  }

  String path =
    "/ws/"
    "google.ai.generativelanguage."
    "v1beta.GenerativeService."
    "BidiGenerateContent?key=";

  path +=
    GEMINI_API_KEY;

  String wsKey =
    createWebSocketKey();

  // ==================================================
  // RAW HTTP WEBSOCKET HANDSHAKE
  // ==================================================

  wsClient.print(
    "GET "
  );

  wsClient.print(
    path
  );

  wsClient.print(
    " HTTP/1.1\r\n"
  );

  wsClient.print(
    "Host: "
  );

  wsClient.print(
    GEMINI_HOST
  );

  wsClient.print(
    "\r\n"
  );

  wsClient.print(
    "Upgrade: websocket\r\n"
  );

  wsClient.print(
    "Connection: Upgrade\r\n"
  );

  wsClient.print(
    "Sec-WebSocket-Version: 13\r\n"
  );

  wsClient.print(
    "Sec-WebSocket-Key: "
  );

  wsClient.print(
    wsKey
  );

  wsClient.print(
    "\r\n"
  );

  wsClient.print(
    "User-Agent: ESP32-S3\r\n"
  );

  wsClient.print(
    "\r\n"
  );

  // ==================================================
  // READ HTTP RESPONSE
  // ==================================================

  unsigned long timer =
    millis();

  while (
    !wsClient.available()
  ) {

    if (
      millis() - timer >
      8000
    ) {

      showMessage(
        "Gemini ERROR",
        "Handshake timeout"
      );

      wsClient.stop();

      return false;
    }

    delay(1);
  }

  String status =
    wsClient.readStringUntil(
      '\n'
    );

  status.trim();

  Serial.print(
    "Handshake: "
  );

  Serial.println(
    status
  );

  if (
    status.indexOf(
      "101"
    ) < 0
  ) {

    showMessage(
      "Gemini ERROR",
      status.substring(
        0,
        20
      )
    );

    wsClient.stop();

    return false;
  }

  // Read remaining headers.

  while (
    wsClient.connected()
  ) {

    String line =
      wsClient.readStringUntil(
        '\n'
      );

    line.trim();

    if (
      line.length() == 0
    ) {

      break;
    }
  }

  wsConnected = true;

  showMessage(
    "Gemini connected",
    "Setting up..."
  );

  // ==================================================
  // SEND SETUP
  // ==================================================

  if (
    !sendGeminiSetup()
  ) {

    showMessage(
      "Gemini ERROR",
      "Setup send failed"
    );

    wsClient.stop();

    wsConnected = false;

    return false;
  }

  // ==================================================
  // WAIT FOR setupComplete
  // ==================================================

  timer =
    millis();

  while (
    wsConnected &&
    !geminiReady &&
    millis() - timer <
    10000
  ) {

    wsPoll();

    delay(1);
  }

  if (
    !geminiReady
  ) {

    showMessage(
      "Setup timeout",
      "Will reconnect"
    );

    Serial.println(
      "Gemini setup timeout"
    );

    wsClient.stop();

    wsConnected = false;

    return false;
  }

  return true;
}

// =====================================================
// SEND ACTIVITY START
// =====================================================

void sendActivityStart() {

  wsSendText(
    "{"
      "\"realtimeInput\":{"
        "\"activityStart\":{}"
      "}"
    "}"
  );
}

// =====================================================
// SEND ACTIVITY END
// =====================================================

void sendActivityEnd() {

  wsSendText(
    "{"
      "\"realtimeInput\":{"
        "\"activityEnd\":{}"
      "}"
    "}"
  );
}

// =====================================================
// SEND MICROPHONE CHUNK
// =====================================================

bool sendAudioChunk() {

  if (
    !recording ||
    !geminiReady ||
    !wsConnected
  ) {

    return false;
  }

  size_t bytesRead = 0;

  if (
    i2s_read(
      I2S_PORT,
      i2sBuffer,
      sizeof(i2sBuffer),
      &bytesRead,
      pdMS_TO_TICKS(120)
    ) != ESP_OK
  ) {

    return false;
  }

  int sampleCount =
    bytesRead /
    sizeof(int32_t);

  // ==================================================
  // 32 BIT I2S → 16 BIT PCM
  // ==================================================

  for (
    int i = 0;
    i < sampleCount;
    i++
  ) {

    int32_t value =
      i2sBuffer[i] >>
      MIC_SHIFT;

    value *=
      MIC_GAIN;

    if (value > 32767) {
      value = 32767;
    }

    if (value < -32768) {
      value = -32768;
    }

    pcmBuffer[i] =
      (int16_t)value;
  }

  size_t pcmBytes =
    sampleCount *
    sizeof(int16_t);

  // ==================================================
  // BASE64
  // ==================================================

  size_t encodedLength = 0;

  if (
    mbedtls_base64_encode(
      base64Buffer,
      sizeof(base64Buffer) - 1,
      &encodedLength,
      (unsigned char*)
        pcmBuffer,
      pcmBytes
    ) != 0
  ) {

    return false;
  }

  base64Buffer[
    encodedLength
  ] = 0;

  // ==================================================
  // REALTIME INPUT
  // ==================================================

  audioMessage =
    "{"
      "\"realtimeInput\":{"
        "\"audio\":{"
          "\"data\":\"";

  audioMessage +=
    (char*)base64Buffer;

  audioMessage +=
          "\","
          "\"mimeType\":"
          "\"audio/pcm;rate=16000\""
        "}"
      "}"
    "}";

  bool result =
    wsSendText(
      audioMessage
    );

  // Check for incoming transcript.
  wsPoll();

  return result;
}

// =====================================================
// START RECORDING
// =====================================================

void startRecording() {

  if (
    !geminiReady
  ) {

    showMessage(
      "Gemini not ready",
      "Please wait"
    );

    return;
  }

  finalTranscript = "";
  interimTranscript = "";

  waitingFinal = false;

  i2s_zero_dma_buffer(
    I2S_PORT
  );

  // Google manual push-to-talk.
  sendActivityStart();

  recording = true;

  Serial.println(
    "Recording START"
  );

  showMessage(
    "LISTENING...",
    "Speak now",
    "Release = finish"
  );
}

// =====================================================
// STOP RECORDING
// =====================================================

void stopRecording() {

  if (!recording) {

    return;
  }

  recording = false;

  sendActivityEnd();

  Serial.println(
    "Recording STOP"
  );

  waitingFinal = true;

  finalTimer =
    millis();

  showMessage(
    "FINALIZING...",
    "Please wait"
  );
}

// =====================================================
// BUTTON HANDLER
// =====================================================

void handleButton() {

  bool reading =
    digitalRead(
      BUTTON_PIN
    );

  if (
    reading !=
    lastButtonReading
  ) {

    debounceTimer =
      millis();

    lastButtonReading =
      reading;
  }

  if (
    millis() -
    debounceTimer >
    DEBOUNCE_TIME
  ) {

    if (
      reading !=
      stableButtonState
    ) {

      stableButtonState =
        reading;

      if (
        stableButtonState ==
        LOW
      ) {

        startRecording();
      }

      else {

        stopRecording();
      }
    }
  }
}

// =====================================================
// FINAL TIMEOUT
// =====================================================

void checkFinalTimeout() {

  if (
    !waitingFinal
  ) {

    return;
  }

  if (
    millis() -
    finalTimer <
    FINAL_TIMEOUT
  ) {

    return;
  }

  waitingFinal = false;

  if (
    finalTranscript.length()
  ) {

    showText(
      "TRANSCRIPT:",
      finalTranscript
    );
  }

  else if (
    interimTranscript.length()
  ) {

    showText(
      "TRANSCRIPT:",
      interimTranscript
    );
  }

  else {

    showMessage(
      "No speech found",
      "Try again"
    );
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(
    115200
  );

  delay(1500);

  Serial.println();
  Serial.println(
    "Gemini Live STT"
  );

  // Button
  pinMode(
    BUTTON_PIN,
    INPUT_PULLUP
  );

  stableButtonState =
    digitalRead(
      BUTTON_PIN
    );

  lastButtonReading =
    stableButtonState;

  // OLED
  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  u8g2.begin();

  showMessage(
    "Speech To Text",
    "Starting..."
  );

  // Reserve memory
  audioMessage.reserve(
    3200
  );

  fragmentedMessage.reserve(
    2048
  );

  interimTranscript.reserve(
    256
  );

  finalTranscript.reserve(
    256
  );

  setupMicrophone();

  connectWiFi();

  connectGemini();

  reconnectTimer =
    millis();
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // ==================================================
  // WIFI
  // ==================================================

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {

    recording = false;

    wsClient.stop();

    wsConnected = false;
    geminiReady = false;

    connectWiFi();
  }

  // ==================================================
  // GEMINI RECONNECT
  // ==================================================

  if (
    !wsConnected
  ) {

    if (
      millis() -
      reconnectTimer >
      RECONNECT_TIME
    ) {

      reconnectTimer =
        millis();

      connectGemini();
    }

    delay(5);

    return;
  }

  // ==================================================
  // RECEIVE GEMINI DATA
  // ==================================================

  wsPoll();

  // ==================================================
  // BUTTON
  // ==================================================

  handleButton();

  // ==================================================
  // SEND AUDIO
  // ==================================================

  if (
    recording &&
    stableButtonState ==
    LOW &&
    geminiReady
  ) {

    sendAudioChunk();
  }

  else {

    delay(1);
  }

  // ==================================================
  // FINAL RESULT
  // ==================================================

  checkFinalTimeout();
}
