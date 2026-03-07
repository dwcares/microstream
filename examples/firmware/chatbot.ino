#include "Microstream.h"

// --- Hardware Pins (Photon) ---
#define LED_PIN      D0       // Onboard LED (no external wiring needed)
#define BUTTON_PIN   D3
#define MIC_PIN      A6
#define SPEAKER_PIN  A3       // True DAC output

// --- Server ---
#define SERVER_HOST "192.168.7.130"
#define SERVER_PORT 5000
#define SERVER_PATH "/"

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
  cfg.sampleRate = 8000;   // 8kHz - easier on limited RAM
  cfg.bitDepth = 16;       // 16-bit signed PCM (standard WAV format)
  cfg.micPin = MIC_PIN;
  cfg.speakerPin = SPEAKER_PIN;
  cfg.captureBufferSize = 8192;   // 8KB capture buffer (4KB samples at 16-bit)
  cfg.playbackBufferSize = 20000; // 20KB playback - ~1.25 seconds at 8kHz 16-bit

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

  // --- LED feedback ---
  if (mic.isRecording()) {
    digitalWrite(LED_PIN, HIGH);
  } else if (mic.isConnected()) {
    digitalWrite(LED_PIN, (millis() / 1000) % 2 ? HIGH : LOW);
  } else {
    digitalWrite(LED_PIN, LOW);
  }

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
