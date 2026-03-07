#ifndef MICROSTREAM_AUDIOPLAYBACK_H
#define MICROSTREAM_AUDIOPLAYBACK_H

#include "application.h"
#include "RingBuffer.h"

// Callback type for LED updates during playback
typedef void (*LevelCallback)(uint8_t level);

// Callback type for periodic updates during blocking playback
typedef void (*TickCallback)();

/**
 * Non-blocking audio playback to a PWM speaker pin.
 *
 * Call update() frequently from the main loop - it plays one sample
 * per call if enough time has elapsed since the last sample.
 *
 * Uses 16-bit signed PCM samples for better audio quality.
 */
class AudioPlayback {
public:
  AudioPlayback();

  void begin(pin_t speakerPin, unsigned int sampleRate, unsigned int bufferSize = 32768);

  // Buffer incoming audio data for playback (16-bit signed samples)
  void feed(int16_t sample);
  void feed(const int16_t* data, unsigned int len);

  // Call frequently from loop() - plays samples at the correct rate
  // Non-blocking: returns immediately after playing 0 or 1 sample
  void update();

  // Old blocking play() - kept for compatibility but use update() instead
  bool play();

  bool isPlaying() const;
  bool hasBufferedData() const;

  // Get smoothed playback level for LED visualization (0-255)
  uint8_t getLevel() const;

  // Set callback for real-time level updates during blocking play()
  void onLevelChange(LevelCallback cb);

  // Set callback for periodic updates during blocking play() (for LED breathing, etc.)
  void onTick(TickCallback cb);

  RingBuffer& buffer();

private:
  pin_t _speakerPin;
  unsigned int _sampleRate;
  unsigned int _timingMicros;
  bool _playing;
  bool _pwmInitialized;
  unsigned long _lastSampleTime;
  RingBuffer _buffer;
  uint8_t _currentLevel;      // Smoothed amplitude for LED visualization
  uint8_t _peakLevel;         // Peak amplitude in current window
  uint16_t _sampleCount;      // Samples since last level update
  LevelCallback _levelCallback;
  TickCallback _tickCallback;
};

#endif
