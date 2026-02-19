#ifndef MICROSTREAM_AUDIOPLAYBACK_H
#define MICROSTREAM_AUDIOPLAYBACK_H

#include "application.h"
#include "RingBuffer.h"

/**
 * Non-blocking audio playback to a PWM speaker pin.
 *
 * Call update() frequently from the main loop - it plays one sample
 * per call if enough time has elapsed since the last sample.
 */
class AudioPlayback {
public:
  AudioPlayback();

  void begin(pin_t speakerPin, unsigned int sampleRate, unsigned int bufferSize = 32768);

  // Buffer incoming audio data for playback
  void feed(uint8_t sample);
  void feed(const uint8_t* data, unsigned int len);

  // Call frequently from loop() - plays samples at the correct rate
  // Non-blocking: returns immediately after playing 0 or 1 sample
  void update();

  // Old blocking play() - kept for compatibility but use update() instead
  bool play();

  bool isPlaying() const;
  bool hasBufferedData() const;

  RingBuffer& buffer();

private:
  pin_t _speakerPin;
  unsigned int _sampleRate;
  unsigned int _timingMicros;
  bool _playing;
  bool _pwmInitialized;
  unsigned long _lastSampleTime;
  RingBuffer _buffer;
};

#endif
