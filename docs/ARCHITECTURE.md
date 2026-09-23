# Architecture

## Overview

This project is designed as a low-latency push-to-talk pipeline.

```text
                         Wi-Fi
                           |
                           v
+-----------+      +---------------+      +--------------------+
| I2S Mic   | ---> |   ESP32-S3    | ---> | Gemini Live API    |
| 16 kHz    |      | audio stream  |      | speech recognition |
+-----------+      +-------+-------+      +----------+---------+
                          ^                         |
                          |                         |
                    push button                    |
                          |                         v
                          +------------------ final/interim text
                                                    |
                                                    v
                                             +-------------+
                                             | SH1106 OLED |
                                             +-------------+
```

## Push-to-talk state flow

```text
BOOT
 |
 v
CONNECT WIFI
 |
 v
OPEN TLS + WEBSOCKET
 |
 v
SEND GEMINI SETUP
 |
 v
WAIT FOR setupComplete
 |
 v
READY
 |
 +---- button pressed ----> START ACTIVITY
 |                            |
 |                            v
 |                       STREAM AUDIO
 |                            |
 |                      button released
 |                            |
 |                            v
 |                       END ACTIVITY
 |                            |
 |                            v
 |                     FINAL TRANSCRIPT
 |                            |
 +----------------------------+
```

## Audio format

The microphone is read through I2S.

Target format sent to Gemini:

- Mono
- 16 kHz
- Signed 16-bit PCM
- Little-endian
- Small chunks for low latency

The microphone may supply higher-resolution samples inside 32-bit I2S words. Firmware reduces/scales them to 16-bit PCM.

## WebSocket transport

The project uses a secure WebSocket over TLS.

The client:

1. Opens TLS to Google's Gemini endpoint.
2. Performs the WebSocket HTTP upgrade.
3. Sends the Gemini setup message.
4. Waits for `setupComplete`.
5. Streams audio during button hold.
6. Receives transcription messages.
7. Finalizes the utterance on button release.

## Binary-frame handling

A critical implementation detail is that Gemini JSON may arrive inside a binary WebSocket frame.

Therefore the receive code must support:

```text
0x01 -> Text frame
0x02 -> Binary frame
0x00 -> Continuation frame
0x08 -> Close
0x09 -> Ping
0x0A -> Pong
```

Both 0x01 and 0x02 payloads should be interpreted as JSON when they contain Gemini server messages.

## Why streaming is faster than file upload

File-based transcription:

```text
record -> stop -> build WAV -> upload -> process -> transcript
```

Live transcription:

```text
record + upload + process happen at the same time
```

By the time the button is released, most of the utterance may already have been transmitted and processed.

