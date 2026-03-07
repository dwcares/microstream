#ifndef MICROSTREAM_RINGBUFFER_H
#define MICROSTREAM_RINGBUFFER_H

#include "application.h"

/**
 * Thread-safe(ish) circular buffer for 16-bit audio samples.
 *
 * Improvements over the original SimpleRingBuffer:
 * - Tracks overflow count so callers know when samples were lost
 * - Uses volatile for fields accessed from ISR context
 * - Properly handles concurrent put() from ISR and get() from main loop
 * - Stores 16-bit signed samples for better audio quality
 */
class RingBuffer {
public:
  RingBuffer();
  ~RingBuffer();

  void init(unsigned int capacity);
  void destroy();

  bool put(int16_t value);
  int16_t get();

  void clear();

  volatile unsigned int getSize() const;
  unsigned int getCapacity() const;

  unsigned long getOverflowCount() const;
  void resetOverflowCount();

private:
  int16_t* _data;
  unsigned int _capacity;
  volatile unsigned int _head;  // read position
  volatile unsigned int _tail;  // write position
  volatile unsigned int _count; // current number of items
  unsigned long _overflowCount;
};

#endif
