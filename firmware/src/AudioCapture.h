#ifndef MICROSTREAM_AUDIOCAPTURE_H
#define MICROSTREAM_AUDIOCAPTURE_H

#include "application.h"
#include "RingBuffer.h"

/**
 * Polling-based audio capture from an analog microphone.
 *
 * Call capture() repeatedly from loop() to sample the ADC at the configured rate.
 * Maps the 12-bit ADC value to 8-bit and stores in a ring buffer.
 *
 * Compatible with both Gen 2 (Photon) and Gen 3 (Argon) devices.
 */
class AudioCapture {
public:
  AudioCapture();

  void begin(pin_t micPin, unsigned int sampleRate, unsigned int bufferSize = 8192);

  void startCapture();
  void stopCapture();

  // Call this frequently from loop() while capturing
  void capture();

  bool isCapturing() const;

  // Access the ring buffer directly for reading samples
  RingBuffer& buffer();

  unsigned long getOverflowCount() const;

private:
  pin_t _micPin;
  unsigned int _sampleRate;
  unsigned int _timingMicros; // microseconds between samples
  bool _capturing;
  unsigned long _lastSampleTime;

  RingBuffer _buffer;
};

#endif
