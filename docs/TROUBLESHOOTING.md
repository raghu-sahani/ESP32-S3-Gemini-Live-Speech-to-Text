# Troubleshooting

## OLED shows random pixels or corrupted text

The display is probably an SH1106 rather than SSD1306.

Use U8g2 with:

```cpp
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);
```

and:

```cpp
Wire.begin(6, 5);
```

## OLED stays blank

Check:

- VCC -> 3.3V
- GND -> GND
- SDA -> GPIO 6
- SCL -> GPIO 5
- I2C address/module type
- U8g2 initialization

Test the OLED by itself before running the full project.

## Microphone is too quiet

Check:

```text
VDD -> 3.3V
GND -> GND
SCK -> GPIO 7
WS  -> GPIO 8
SD  -> GPIO 9
L/R -> GND
```

If L/R is tied to GND, the firmware should normally read the left I2S channel.

Also check microphone sample scaling/gain.

A useful diagnostic is to calculate the maximum absolute PCM sample while speaking.

Very small values near zero usually indicate a wiring/channel problem.

Constant values near 32767 indicate clipping or excessive gain.

## Transcript is inaccurate

Try:

- Speak closer to the microphone.
- Lower background noise.
- Reduce clipping.
- Speak for at least about one second.
- Confirm 16-kHz mono PCM is being sent.
- Verify the I2S channel matches the L/R pin state.

## Device stays at "Gemini connected / Setting up"

First verify the API outside the ESP32 using a small Python WebSocket test.

A successful response looks like:

```text
b'{
  "setupComplete": {}
}'
```

If Python works but ESP32 does not, inspect WebSocket frames.

Important: Gemini may send JSON as a **binary WebSocket frame**.

Handle both:

```text
opcode 0x01 -> text
opcode 0x02 -> binary
```

Ignoring opcode 0x02 can make the ESP32 wait forever even though Google already returned `setupComplete`.

## Gemini keeps disconnecting

Check:

- Wi-Fi stability
- Correct Gemini API key
- Correct WebSocket endpoint
- TLS connection
- Correct WebSocket masking for client frames
- Ping/Pong handling
- Close code and close reason printed to Serial Monitor

## No Serial Monitor output

For many ESP32-S3 boards:

```text
USB CDC On Boot -> Enabled
Serial Monitor -> 115200 baud
```

After uploading, check the COM port again because native USB boards can reconnect under a different port.

Press RESET once with Serial Monitor open.

## Upload fails

Try:

1. Disconnect power from unnecessary peripherals.
2. Hold BOOT.
3. Press RESET.
4. Release RESET.
5. Release BOOT.
6. Upload.
7. Confirm the selected COM port.

## "No speech found"

Check that:

- Button stays held while speaking.
- Audio chunks are actually being sent.
- Microphone peak is above near-zero noise.
- Gemini setup completed.
- Activity start/end or equivalent stream finalization is sent correctly.

## API key exposed accidentally

Revoke the exposed key immediately in Google AI Studio, create a new key, and replace it only in your local firmware.

Never try to "hide" a leaked key by deleting the GitHub file; Git history can preserve it.

