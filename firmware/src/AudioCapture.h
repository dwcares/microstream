#ifndef MICROSTREAM_AUDIOCAPTURE_H
#define MICROSTREAM_AUDIOCAPTURE_H

#include "application.h"
#include "RingBuffer.h"

/**
 * Polling-based audio capture from an analog microphone.
 *
 * Call capture() repeatedly from loop() to sample the ADC at the configured rate.
 * Outputs 16-bit signed PCM samples (centered at 0) stored in a ring buffer.
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

  // DC offset calibration - measured at startup
  int16_t _adcMid;

  // Simple IIR low-pass filter state for anti-aliasing
  // Cutoff ~3kHz at 8kHz sample rate (alpha ~0.7)
  int32_t _filterState;
  static const int32_t FILTER_ALPHA = 180;  // 0.7 * 256 ≈ 180

  // DC-blocking high-pass filter to remove drift
  // y[n] = alpha * (y[n-1] + x[n] - x[n-1])
  // Alpha ~0.995 gives cutoff ~25Hz at 8kHz (removes DC, keeps voice)
  int32_t _hpfPrevInput;
  int32_t _hpfPrevOutput;
  static const int32_t HPF_ALPHA = 254;  // 0.995 * 256 ≈ 254
};

#endif
