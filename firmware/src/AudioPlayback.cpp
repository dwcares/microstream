#include "AudioPlayback.h"

AudioPlayback::AudioPlayback()
  : _speakerPin(A3), _sampleRate(8000), _timingMicros(125), _playing(false), _pwmInitialized(false), _lastSampleTime(0) {}

void AudioPlayback::begin(pin_t speakerPin, unsigned int sampleRate, unsigned int bufferSize) {
  _speakerPin = speakerPin;
  _sampleRate = sampleRate;
  _timingMicros = 1000000 / sampleRate;

  pinMode(_speakerPin, OUTPUT);
  _buffer.init(bufferSize);

  // Set DAC to midpoint (silence)
  analogWrite(_speakerPin, 2048);
  _lastSampleTime = micros();
}

void AudioPlayback::feed(uint8_t sample) {
  _buffer.put(sample);
}

void AudioPlayback::feed(const uint8_t* data, unsigned int len) {
  for (unsigned int i = 0; i < len; i++) {
    _buffer.put(data[i]);
  }
}

void AudioPlayback::update() {
  // Non-blocking: play one sample if timing allows
  if (_buffer.getSize() == 0) {
    if (_playing) {
      analogWrite(_speakerPin, 2048);  // Return to silence
      _playing = false;
    }
    return;
  }

  unsigned long now = micros();
  if (now - _lastSampleTime >= _timingMicros) {
    uint8_t value = _buffer.get();
    // Map 8-bit (0-255) to 12-bit DAC (0-4095)
    int dacValue = map(value, 0, 255, 0, 4095);
    analogWrite(_speakerPin, dacValue);
    _lastSampleTime = now;
    _playing = true;
  }
}

bool AudioPlayback::play() {
  if (_buffer.getSize() == 0) {
    _playing = false;
    return false;
  }

  _playing = true;

  // Blocking playback with tight timing
  while (_buffer.getSize() > 0) {
    uint8_t value = _buffer.get();

    // Busy-wait for precise timing
    while (micros() - _lastSampleTime < _timingMicros) {
      // spin
    }
    _lastSampleTime = micros();

    // Map 8-bit (0-255) to 12-bit DAC (0-4095)
    int dacValue = map(value, 0, 255, 0, 4095);
    analogWrite(_speakerPin, dacValue);
  }

  // Return to silence
  analogWrite(_speakerPin, 2048);
  _playing = false;
  return true;
}

bool AudioPlayback::isPlaying() const {
  return _playing;
}

bool AudioPlayback::hasBufferedData() const {
  return _buffer.getSize() > 0;
}

RingBuffer& AudioPlayback::buffer() {
  return _buffer;
}
