/*
 * Microstream Chatbot Example
 *
 * Voice assistant demo with LED status feedback and callbacks.
 * Press and hold the button to talk, release to send.
 * LED indicates connection status.
 *
 * Hardware:
 *   - Microphone (with amp) on MIC_PIN
 *   - Speaker (with amp) on SPEAKER_PIN
 *   - Push button on BUTTON_PIN (active low)
 *   - LED on LED_PIN (or use onboard D7)
 */

#include "Microstream.h"

// --- Hardware Pins (Photon) ---
#define LED_PIN      D0       // Status LED
#define BUTTON_PIN   D3       // Push-to-talk button
#define MIC_PIN      A0       // Microphone input
#define SPEAKER_PIN  A3       // Speaker output (DAC)

// --- Server Configuration ---
#define SERVER_HOST  "192.168.1.100"  // Your server IP
#define SERVER_PORT  5000
#define SERVER_PATH  "/"

Microstream mic;

bool buttonDown = false;
unsigned long lastStatusTime = 0;
unsigned long connectedSince = 0;

// --- Callbacks ---
void onMicConnected() {
  connectedSince = millis();
  Serial.println("Connected to server");
}

void onMicDisconnected() {
  connectedSince = 0;
  Serial.println("Disconnected from server");
}

void onPlaybackStart() {
  Serial.println("Playing response...");
}

void onPlaybackEnd() {
  Serial.println("Playback done");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("--- Chatbot ---");

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  MicrostreamConfig cfg;
  cfg.sampleRate = 16000;  // 16kHz - better voice quality
  cfg.bitDepth = 16;       // 16-bit signed PCM (standard WAV format)
  cfg.micPin = MIC_PIN;
  cfg.speakerPin = SPEAKER_PIN;
  cfg.captureBufferSize = 8192;   // 8K samples (~0.5 sec at 16kHz)
  cfg.playbackBufferSize = 8192;  // 8K samples (~0.5 sec at 16kHz)

  Serial.printlnf("Connecting to %s:%d%s", SERVER_HOST, SERVER_PORT, SERVER_PATH);

  mic.begin(SERVER_HOST, SERVER_PORT, SERVER_PATH, cfg);
  mic.onConnected(onMicConnected);
  mic.onDisconnected(onMicDisconnected);
  mic.onPlaybackStart(onPlaybackStart);
  mic.onPlaybackEnd(onPlaybackEnd);
}

void loop() {
  mic.update();

  // --- Push-to-talk button ---
  bool pressed = (digitalRead(BUTTON_PIN) == LOW);

  if (pressed && !buttonDown && mic.isConnected()) {
    buttonDown = true;
    mic.startRecording();
    Serial.println("Recording...");
  }

  if (!pressed && buttonDown) {
    buttonDown = false;
    if (mic.isRecording()) {
      mic.stopRecording();
      Serial.println("Sent audio, waiting for response...");
    }
  }

  // --- LED feedback: on = connected, off = disconnected ---
  digitalWrite(LED_PIN, mic.isConnected() ? HIGH : LOW);

  // --- Status ---
  if (millis() - lastStatusTime > 5000) {
    lastStatusTime = millis();
    if (mic.isConnected()) {
      Serial.printlnf("Status: CONNECTED | Uptime: %lus", (millis() - connectedSince) / 1000);
    } else {
      Serial.println("Status: DISCONNECTED");
    }
  }
}
