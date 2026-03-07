#include "AudioCapture.h"

AudioCapture::AudioCapture()
  : _micPin(A0), _sampleRate(16000), _timingMicros(62), _capturing(false), _lastSampleTime(0), _adcMid(2048), _filterState(2048 << 8), _hpfPrevInput(0), _hpfPrevOutput(0) {}

void AudioCapture::begin(pin_t micPin, unsigned int sampleRate, unsigned int bufferSize) {
  _micPin = micPin;
  _sampleRate = sampleRate;
  _timingMicros = 1000000 / sampleRate; // e.g., 1000000/16000 = 62 us

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
  _hpfPrevInput = 0;
  _hpfPrevOutput = 0;
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
    // y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
    // Using 8-bit fixed point: alpha = 180/256 ≈ 0.7, cutoff ~3kHz at 8kHz
    int32_t input = raw << 8;  // Scale up for fixed-point
    _filterState = ((FILTER_ALPHA * input) + ((256 - FILTER_ALPHA) * _filterState)) >> 8;
    uint16_t filtered = _filterState >> 8;  // Scale back down

    // Convert to 16-bit signed using calibrated center point
    int32_t centered = ((int32_t)filtered - _adcMid) << 4;

    // DC-blocking high-pass filter to remove any remaining drift
    // y[n] = alpha * (y[n-1] + x[n] - x[n-1])
    int32_t hpfOutput = (HPF_ALPHA * (_hpfPrevOutput + centered - _hpfPrevInput)) >> 8;
    _hpfPrevInput = centered;
    _hpfPrevOutput = hpfOutput;

    // Clamp to 16-bit signed range
    int16_t sample16 = (int16_t)constrain(hpfOutput, -32768, 32767);

    // Store as two bytes, little-endian
    _buffer.put((uint8_t)(sample16 & 0xFF));         // Low byte
    _buffer.put((uint8_t)((sample16 >> 8) & 0xFF));  // High byte

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
