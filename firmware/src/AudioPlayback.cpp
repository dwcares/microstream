#include "AudioPlayback.h"

AudioPlayback::AudioPlayback()
  : _speakerPin(A3), _sampleRate(8000), _timingMicros(125), _playing(false), _pwmInitialized(false), _lastSampleTime(0), _currentLevel(0), _peakLevel(0), _sampleCount(0), _levelCallback(NULL) {}

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

void AudioPlayback::feed(int16_t sample) {
  _buffer.put(sample);
}

void AudioPlayback::feed(const int16_t* data, unsigned int len) {
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
      _currentLevel = 0;
      _peakLevel = 0;
      _sampleCount = 0;
    }
    return;
  }

  unsigned long now = micros();
  if (now - _lastSampleTime >= _timingMicros) {
    int16_t value = _buffer.get();
    // Map 16-bit signed (-32768 to 32767) to 12-bit DAC (0-4095)
    int dacValue = map(value, -32768, 32767, 0, 4095);
    analogWrite(_speakerPin, dacValue);
    _lastSampleTime = now;
    _playing = true;

    // Track peak amplitude over a window (~50ms at 8kHz = 400 samples)
    // For 16-bit signed, amplitude is absolute value scaled to 0-255
    int16_t absVal = (value >= 0) ? value : -value;
    uint8_t amplitude = (uint8_t)(absVal >> 7);  // Scale 32768 -> 255
    if (amplitude > _peakLevel) {
      _peakLevel = amplitude;
    }
    _sampleCount++;

    // Update level every ~50ms for visible LED changes
    if (_sampleCount >= 400) {
      // Use peak from this window, with slight smoothing for decay
      if (_peakLevel > _currentLevel) {
        _currentLevel = _peakLevel;  // Fast attack
      } else {
        _currentLevel = (_currentLevel + _peakLevel) / 2;  // Faster decay
      }
      _peakLevel = 0;
      _sampleCount = 0;
    }
  }
}

bool AudioPlayback::play() {
  if (_buffer.getSize() == 0) {
    _playing = false;
    return false;
  }

  _playing = true;
  uint8_t lastReportedLevel = 0;

  // Blocking playback with tight timing
  while (_buffer.getSize() > 0) {
    int16_t value = _buffer.get();

    // Busy-wait for precise timing
    while (micros() - _lastSampleTime < _timingMicros) {
      // spin
    }
    _lastSampleTime = micros();

    // Map 16-bit signed (-32768 to 32767) to 12-bit DAC (0-4095)
    int dacValue = map(value, -32768, 32767, 0, 4095);
    analogWrite(_speakerPin, dacValue);

    // Track level for LED visualization
    int16_t absVal = (value >= 0) ? value : -value;
    uint8_t amplitude = (uint8_t)(absVal >> 7);
    if (amplitude > _peakLevel) {
      _peakLevel = amplitude;
    }
    _sampleCount++;
    if (_sampleCount >= 400) {
      if (_peakLevel > _currentLevel) {
        _currentLevel = _peakLevel;
      } else {
        _currentLevel = (_currentLevel + _peakLevel) / 2;
      }
      _peakLevel = 0;
      _sampleCount = 0;

      // Notify callback for real-time LED update during blocking playback
      if (_levelCallback && _currentLevel != lastReportedLevel) {
        _levelCallback(_currentLevel);
        lastReportedLevel = _currentLevel;
      }
    }
  }

  // Return to silence
  analogWrite(_speakerPin, 2048);
  _playing = false;
  if (_levelCallback) {
    _levelCallback(0);
  }
  _currentLevel = 0;
  _peakLevel = 0;
  _sampleCount = 0;
  return true;
}

bool AudioPlayback::isPlaying() const {
  return _playing;
}

bool AudioPlayback::hasBufferedData() const {
  return _buffer.getSize() > 0;
}

uint8_t AudioPlayback::getLevel() const {
  return _currentLevel;
}

void AudioPlayback::onLevelChange(LevelCallback cb) {
  _levelCallback = cb;
}

RingBuffer& AudioPlayback::buffer() {
  return _buffer;
}
