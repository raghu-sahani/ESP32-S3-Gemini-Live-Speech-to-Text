# Wiring Guide

## ESP32-S3 to SH1106 OLED

| OLED pin | ESP32-S3 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 6 |
| SCL/SCK | GPIO 5 |

The display used in this project is a 128x64 SH1106 I2C OLED.

The tested U8g2 constructor is:

```cpp
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);
```

and I2C is initialized with:

```cpp
Wire.begin(6, 5); // SDA, SCL
```

## ESP32-S3 to I2S microphone

| Microphone pin | ESP32-S3 |
|---|---|
| VDD | 3.3V |
| GND | GND |
| SCK/BCLK | GPIO 7 |
| WS/LRCLK | GPIO 8 |
| SD | GPIO 9 |
| L/R | GND |

With L/R tied to GND, configure the microphone as the left I2S channel.

```cpp
config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
```

If your microphone module uses the opposite L/R convention, verify its datasheet before changing the channel.

## Push button

| Button | ESP32-S3 |
|---|---|
| Side 1 | GPIO 4 |
| Side 2 | GND |

Configure GPIO 4 as:

```cpp
pinMode(4, INPUT_PULLUP);
```

Logic:

```text
Released = HIGH
Pressed  = LOW
```

No external pull-up resistor is needed.

## Power

Use the ESP32-S3 3.3V output for the OLED and microphone if your modules are designed for 3.3V operation.

All grounds must be common:

```text
ESP32 GND
   |
   +---- OLED GND
   |
   +---- MIC GND
   |
   +---- Push button
```

## Connection checklist

Before powering the circuit:

- Confirm OLED VCC is not connected to GND.
- Confirm microphone VDD is 3.3V.
- Confirm all grounds are shared.
- Confirm SDA/SCL are not reversed.
- Confirm microphone SCK, WS, and SD are on GPIO 7/8/9.
- Confirm L/R is not floating.
- Confirm the button connects GPIO 4 to GND only when pressed.

