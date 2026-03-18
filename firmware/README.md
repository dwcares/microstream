# Microstream

Bidirectional audio streaming library for Particle microcontrollers (Photon, Argon). Stream audio between your Particle device and a Node.js server over TCP/WebSocket.

## Features

- **Push-to-talk audio capture** - Interrupt-driven ADC sampling from microphone
- **Real-time audio playback** - Timed DAC output to speaker with ring buffer
- **Automatic reconnection** - Exponential backoff with configurable delays
- **Binary protocol** - Efficient framing with heartbeat keep-alive
- **Event callbacks** - Connection, playback start/end notifications

## Installation

```
particle library add microstream
```

Or add to your `project.properties`:

```properties
dependencies.microstream=0.1.0
```

## Quick Start

```cpp
#include "Microstream.h"

#define MIC_PIN      A0
#define SPEAKER_PIN  A3
#define BUTTON_PIN   D3

Microstream stream;
bool buttonDown = false;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  MicrostreamConfig cfg;
  cfg.sampleRate = 16000;
  cfg.bitDepth = 16;
  cfg.micPin = MIC_PIN;
  cfg.speakerPin = SPEAKER_PIN;
  cfg.captureBufferSize = 8192;
  cfg.playbackBufferSize = 8192;

  stream.begin("your-server.local", 5000, "/", cfg);
}

void loop() {
  stream.update();

  bool pressed = (digitalRead(BUTTON_PIN) == LOW);

  // Start recording on button press
  if (pressed && !buttonDown && stream.isConnected()) {
    buttonDown = true;
    stream.startRecording();
  }

  // Stop recording on button release
  if (!pressed && buttonDown) {
    buttonDown = false;
    stream.stopRecording();
  }
}
```

## Hardware Requirements

| Component | Pin | Notes |
|-----------|-----|-------|
| Microphone | A0-A5 | Electret mic with MAX4466 or similar amp |
| Speaker | A3 or A6 | DAC output (Photon: A3, Argon: A6) |
| Button | Any GPIO | Optional, for push-to-talk |

### Wiring Example (Photon)

```
Electret Mic + MAX4466:
  VCC → 3.3V
  GND → GND
  OUT → A0

Speaker (with amplifier):
  DAC (A3) → Amp Input
  Amp Output → Speaker
```

## API Reference

### MicrostreamConfig

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `sampleRate` | `uint` | 16000 | Audio sample rate in Hz |
| `bitDepth` | `uint8` | 16 | Bits per sample (8 or 16) |
| `micPin` | `pin_t` | - | Analog input pin for microphone |
| `speakerPin` | `pin_t` | - | DAC output pin for speaker |
| `captureBufferSize` | `uint` | 4096 | Ring buffer size for capture (samples) |
| `playbackBufferSize` | `uint` | 4096 | Ring buffer size for playback (samples) |

### Microstream Class

| Method | Description |
|--------|-------------|
| `begin(host, port, path, config)` | Initialize and connect to server |
| `update()` | Call from `loop()` - handles connection, data transfer |
| `startRecording()` | Begin capturing audio from mic |
| `stopRecording()` | Stop capturing and send audio to server |
| `isConnected()` | Returns `true` if connected to server |
| `isRecording()` | Returns `true` if currently recording |
| `isPlaying()` | Returns `true` if audio is playing |
| `getPlaybackLevel()` | Returns current playback amplitude (0-255) |

### Event Callbacks

| Method | Description |
|--------|-------------|
| `onConnected(callback)` | Called when connected to server |
| `onDisconnected(callback)` | Called when disconnected from server |
| `onPlaybackStart(callback)` | Called when audio playback begins |
| `onPlaybackEnd(callback)` | Called when audio playback ends |
| `onPlaybackLevel(callback)` | Called with amplitude during playback |
| `onPlaybackTick(callback)` | Called periodically during playback |

## Server

Use the companion Node.js package to run the server:

```bash
npm install microstream-server
```

See [microstream-server on npm](https://www.npmjs.com/package/microstream-server) for server documentation.

## Examples

- **basic** - Minimal push-to-talk example
- **chatbot** - Voice assistant with status LEDs and callbacks

## License

MIT License - see [LICENSE](LICENSE)
