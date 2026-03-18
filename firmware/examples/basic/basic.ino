/*
 * Microstream Basic Example
 *
 * Minimal push-to-talk demo for Particle Photon/Argon.
 * Press and hold the button to record, release to send audio to server.
 * Server response plays through the speaker automatically.
 *
 * Hardware:
 *   - Microphone (with amp) on MIC_PIN
 *   - Speaker (with amp) on SPEAKER_PIN
 *   - Push button on BUTTON_PIN (active low)
 */

#include "Microstream.h"

// --- Pin Configuration ---
#define MIC_PIN      A0       // Microphone input (any analog pin)
#define SPEAKER_PIN  A3       // Speaker output (DAC: A3 on Photon, A6 on Argon)
#define BUTTON_PIN   D3       // Push-to-talk button (active low)

// --- Server Configuration ---
#define SERVER_HOST  "192.168.1.100"  // Your server IP
#define SERVER_PORT  5000

Microstream stream;
bool buttonDown = false;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  MicrostreamConfig cfg;
  cfg.sampleRate = 16000;
  cfg.bitDepth = 16;
  cfg.micPin = MIC_PIN;
  cfg.speakerPin = SPEAKER_PIN;
  cfg.captureBufferSize = 8192;
  cfg.playbackBufferSize = 8192;

  stream.begin(SERVER_HOST, SERVER_PORT, "/", cfg);
}

void loop() {
  stream.update();

  bool pressed = (digitalRead(BUTTON_PIN) == LOW);

  if (pressed && !buttonDown && stream.isConnected()) {
    buttonDown = true;
    stream.startRecording();
    Serial.println("Recording...");
  }

  if (!pressed && buttonDown) {
    buttonDown = false;
    stream.stopRecording();
    Serial.println("Sent");
  }
}
