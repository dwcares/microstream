#include "AudioCapture.h"

AudioCapture::AudioCapture()
  : _micPin(A0), _sampleRate(16000), _timingMicros(62), _capturing(false), _lastSampleTime(0) {}

void AudioCapture::begin(pin_t micPin, unsigned int sampleRate, unsigned int bufferSize) {
  _micPin = micPin;
  _sampleRate = sampleRate;
  _timingMicros = 1000000 / sampleRate; // e.g., 1000000/16000 = 62 us

  pinMode(_micPin, INPUT);

  _buffer.init(bufferSize);
}

void AudioCapture::startCapture() {
  if (_capturing) return;

  _buffer.clear();
  _buffer.resetOverflowCount();
  _capturing = true;
  _lastSampleTime = micros();
}

void AudioCapture::stopCapture() {
  if (!_capturing) return;
  _capturing = false;
}

void AudioCapture::capture() {
  if (!_capturing) return;

  unsigned long now = micros();

  // Sample as many times as needed to catch up
  while (now - _lastSampleTime >= _timingMicros) {
    uint16_t raw = analogRead(_micPin);
    uint8_t sample = map(raw, 0, 4095, 0, 255);
    _buffer.put(sample);
    _lastSampleTime += _timingMicros;
  }
}

bool AudioCapture::isCapturing() const {
  return _capturing;
}

RingBuffer& AudioCapture::buffer() {
  return _buffer;
}

unsigned long AudioCapture::getOverflowCount() const {
  return _buffer.getOverflowCount();
}
