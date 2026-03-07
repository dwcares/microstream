#include "AudioCapture.h"

AudioCapture::AudioCapture()
  : _micPin(A0), _sampleRate(16000), _timingMicros(62), _timingFrac(500), _fracAccum(0),
    _capturing(false), _lastSampleTime(0), _adcMid(2048), _filterState(2048 << 8) {}

void AudioCapture::begin(pin_t micPin, unsigned int sampleRate, unsigned int bufferSize) {
  _micPin = micPin;
  _sampleRate = sampleRate;

  // Calculate timing with sub-microsecond precision
  // 1000000 / 16000 = 62.5 μs → _timingMicros=62, _timingFrac=500 (out of 1000)
  unsigned long totalNanos = 1000000000UL / sampleRate;  // nanoseconds per sample
  _timingMicros = totalNanos / 1000;                      // integer microseconds
  _timingFrac = totalNanos % 1000;                        // fractional nanoseconds (0-999)
  _fracAccum = 0;

  pinMode(_micPin, INPUT);

  _buffer.init(bufferSize);

  // Calibrate DC offset by measuring the mic's baseline
  int32_t sum = 0;
  for (int i = 0; i < 500; i++) {
    sum += analogRead(_micPin);
    delayMicroseconds(_timingMicros);
  }
  _adcMid = sum / 500;
  _filterState = _adcMid << 8;  // Initialize filter to calibrated center
}

void AudioCapture::startCapture() {
  if (_capturing) return;

  _buffer.clear();
  _buffer.resetOverflowCount();
  _filterState = _adcMid << 8;  // Reset filter to calibrated center
  _fracAccum = 0;
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

    // IIR low-pass filter to prevent aliasing (fixed-point math)
    int32_t input = raw << 8;  // Scale up for fixed-point
    _filterState = ((FILTER_ALPHA * input) + ((256 - FILTER_ALPHA) * _filterState)) >> 8;
    uint16_t filtered = _filterState >> 8;  // Scale back down

    // Convert to 16-bit signed using calibrated center point
    int16_t sample16 = ((int32_t)filtered - _adcMid) << 4;

    // Store 16-bit sample directly in buffer
    _buffer.put(sample16);

    // Advance timing with sub-microsecond accuracy
    _lastSampleTime += _timingMicros;
    _fracAccum += _timingFrac;
    if (_fracAccum >= 1000) {
      _fracAccum -= 1000;
      _lastSampleTime += 1;
    }
  }
}

void AudioCapture::resetTiming() {
  _lastSampleTime = micros();
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
