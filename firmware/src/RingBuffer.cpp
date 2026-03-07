#include "RingBuffer.h"

RingBuffer::RingBuffer()
  : _data(NULL), _capacity(0), _head(0), _tail(0), _count(0), _overflowCount(0) {}

RingBuffer::~RingBuffer() {
  destroy();
}

void RingBuffer::init(unsigned int capacity) {
  destroy();
  _data = (int16_t*)malloc(capacity * sizeof(int16_t));
  _capacity = capacity;
  _head = 0;
  _tail = 0;
  _count = 0;
  _overflowCount = 0;
}

void RingBuffer::destroy() {
  if (_data) {
    free(_data);
    _data = NULL;
  }
  _capacity = 0;
  _head = 0;
  _tail = 0;
  _count = 0;
}

bool RingBuffer::put(int16_t value) {
  if (_count >= _capacity) {
    _overflowCount++;
    return false;
  }
  _data[_tail] = value;
  _tail = (_tail + 1) % _capacity;
  _count++;
  return true;
}

int16_t RingBuffer::get() {
  if (_count == 0) {
    return 0;
  }
  int16_t value = _data[_head];
  _head = (_head + 1) % _capacity;
  _count--;
  return value;
}

void RingBuffer::clear() {
  _head = 0;
  _tail = 0;
  _count = 0;
}

volatile unsigned int RingBuffer::getSize() const {
  return _count;
}

unsigned int RingBuffer::getCapacity() const {
  return _capacity;
}

unsigned long RingBuffer::getOverflowCount() const {
  return _overflowCount;
}

void RingBuffer::resetOverflowCount() {
  _overflowCount = 0;
}
