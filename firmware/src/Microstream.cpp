#include "Microstream.h"

Microstream::Microstream()
  : _port(80),
    _connected(false),
    _recording(false),
    _wasPlaying(false),
    _lastSendTime(0),
    _lastHeartbeatTime(0),
    _lastConnectAttempt(0),
    _reconnectDelay(2000),
    _onConnected(NULL),
    _onDisconnected(NULL),
    _onPlaybackStart(NULL),
    _onPlaybackEnd(NULL)
{
  memset(_host, 0, sizeof(_host));
  memset(_path, 0, sizeof(_path));
}

Microstream::~Microstream() {
  if (_recording) {
    _capture.stopCapture();
  }
}

void Microstream::begin(const char* host, int port, const char* path, MicrostreamConfig config) {
  strncpy(_host, host, sizeof(_host) - 1);
  _port = port;
  strncpy(_path, path, sizeof(_path) - 1);
  _config = config;

  // Default values
  if (_config.sampleRate == 0) _config.sampleRate = 16000;
  if (_config.bitDepth == 0) _config.bitDepth = 16;
  if (_config.captureBufferSize == 0) _config.captureBufferSize = 8192;
  if (_config.playbackBufferSize == 0) _config.playbackBufferSize = 32768;

  _capture.begin(_config.micPin, _config.sampleRate, _config.captureBufferSize);
  _playback.begin(_config.speakerPin, _config.sampleRate, _config.playbackBufferSize);
}

void Microstream::update() {
  // Connection management
  if (!_connected) {
    unsigned long now = millis();
    if (now - _lastConnectAttempt >= _reconnectDelay) {
      _connect();
      _lastConnectAttempt = now;
    }
    return;
  }

  // Check if still connected
  if (!_tcpClient.connected()) {
    _connected = false;
    if (_onDisconnected) _onDisconnected();
    return;
  }

  // Recording: capture samples and send audio periodically
  if (_recording) {
    _capture.capture();  // Poll for new audio samples
    unsigned long now = millis();
    if (now - _lastSendTime >= SEND_INTERVAL_MS) {
      _sendAudio();
      _lastSendTime = now;
    }
  } else {
    // Not recording: receive and play audio
    _receiveAndPlay();

    // Detect playback state transitions
    bool playing = _playback.isPlaying();
    if (playing && !_wasPlaying && _onPlaybackStart) {
      _onPlaybackStart();
    } else if (!playing && _wasPlaying && _onPlaybackEnd) {
      _capture.resetTiming();  // Reset capture timing after playback
      _onPlaybackEnd();
    }
    _wasPlaying = playing;
  }

  // Heartbeat
  unsigned long now = millis();
  if (now - _lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS) {
    _sendHeartbeat();
    _lastHeartbeatTime = now;
  }
}

void Microstream::startRecording() {
  if (_recording || !_connected) return;

  _recording = true;
  _capture.startCapture();
  _lastSendTime = millis();
}

void Microstream::stopRecording() {
  if (!_recording) return;

  _capture.stopCapture();
  _recording = false;

  // Send any remaining audio
  _sendAudio();

  // Send end-of-recording signal
  _sendEnd();
}

bool Microstream::isRecording() const {
  return _recording;
}

bool Microstream::isPlaying() const {
  return _playback.isPlaying();
}

bool Microstream::isConnected() const {
  return _connected;
}

uint8_t Microstream::getPlaybackLevel() const {
  return _playback.getLevel();
}

void Microstream::onPlaybackLevel(void (*cb)(uint8_t level)) {
  _playback.onLevelChange(cb);
}

void Microstream::onConnected(MicrostreamCallback cb) {
  _onConnected = cb;
}

void Microstream::onDisconnected(MicrostreamCallback cb) {
  _onDisconnected = cb;
}

void Microstream::onPlaybackStart(MicrostreamCallback cb) {
  _onPlaybackStart = cb;
}

void Microstream::onPlaybackEnd(MicrostreamCallback cb) {
  _onPlaybackEnd = cb;
}

void Microstream::_connect() {
  if (_tcpClient.connect(_host, _port)) {
    _connected = true;
    _reconnectDelay = 2000; // Reset backoff on success
    _lastHeartbeatTime = millis();

    if (_onConnected) _onConnected();
  } else {
    // Exponential backoff
    _reconnectDelay = min(_reconnectDelay * 2, MAX_RECONNECT_DELAY_MS);
  }
}

void Microstream::_sendAudio() {
  RingBuffer& buf = _capture.buffer();

  // For 16-bit audio, each sample is 2 bytes
  // MIN_SEND_SIZE and MAX_SEND_SIZE are in samples
  while (buf.getSize() >= MIN_SEND_SIZE) {
    unsigned int samples = min((unsigned int)buf.getSize(), MAX_SEND_SIZE / 2);
    unsigned int bytes = samples * 2;

    // TCP protocol: [type (1 byte)][length (2 bytes LE)][payload]
    _txBuffer[0] = MicrostreamProtocol::AUDIO_DATA;
    _txBuffer[1] = (uint8_t)(bytes & 0xFF);        // Length low byte
    _txBuffer[2] = (uint8_t)((bytes >> 8) & 0xFF); // Length high byte

    for (unsigned int i = 0; i < samples; i++) {
      int16_t sample = buf.get();
      // Pack as little-endian (low byte first)
      _txBuffer[3 + i * 2] = (uint8_t)(sample & 0xFF);
      _txBuffer[3 + i * 2 + 1] = (uint8_t)((sample >> 8) & 0xFF);
    }

    _tcpClient.write(_txBuffer, 3 + bytes);
  }
}

void Microstream::_sendEnd() {
  // TCP protocol: [type][length = 0]
  _txBuffer[0] = MicrostreamProtocol::AUDIO_END;
  _txBuffer[1] = 0;
  _txBuffer[2] = 0;
  _tcpClient.write(_txBuffer, 3);
}

void Microstream::_sendHeartbeat() {
  // TCP protocol: [type][length = 0]
  _txBuffer[0] = MicrostreamProtocol::HEARTBEAT;
  _txBuffer[1] = 0;
  _txBuffer[2] = 0;
  _tcpClient.write(_txBuffer, 3);
}

void Microstream::_receiveAndPlay() {
  // Read available data from TCP and handle messages.
  // 16-bit audio: read 2 bytes per sample, little-endian
  while (_tcpClient.available()) {
    uint8_t typeByte = _tcpClient.read();

    switch (typeByte) {
      case MicrostreamProtocol::AUDIO_DATA: {
        // Read 16-bit samples (2 bytes each, little-endian)
        while (_tcpClient.available() >= 2) {
          uint8_t low = _tcpClient.read();
          int next = _tcpClient.peek();
          // Check if next byte is a message type (end of audio data)
          if (next >= MicrostreamProtocol::AUDIO_DATA &&
              next <= MicrostreamProtocol::MSG_ERROR) {
            // Odd byte at end - can't form a sample, discard
            break;
          }
          uint8_t high = _tcpClient.read();
          int16_t sample = (int16_t)((high << 8) | low);
          _playback.feed(sample);
        }
        break;
      }
      case MicrostreamProtocol::HEARTBEAT:
        break;
      case MicrostreamProtocol::CONFIG:
        // Config payload is JSON (bytes >= 0x20), skip it
        while (_tcpClient.available()) {
          int next = _tcpClient.peek();
          if (next >= MicrostreamProtocol::AUDIO_DATA &&
              next <= MicrostreamProtocol::MSG_ERROR) {
            break;
          }
          _tcpClient.read();
        }
        break;
      default:
        break;
    }
  }

  // Blocking playback - plays all buffered samples at correct rate
  _playback.play();
}

void Microstream::_handleMessage(const uint8_t* data, unsigned int len) {
  if (len < 1) return;

  MicrostreamProtocol::MessageType type = MicrostreamProtocol::decodeType(data);

  switch (type) {
    case MicrostreamProtocol::AUDIO_DATA:
      if (len > 1) {
        // 16-bit audio: unpack 2 bytes per sample, little-endian
        unsigned int numSamples = (len - 1) / 2;
        for (unsigned int i = 0; i < numSamples; i++) {
          uint8_t low = data[1 + i * 2];
          uint8_t high = data[1 + i * 2 + 1];
          int16_t sample = (int16_t)((high << 8) | low);
          _playback.feed(sample);
        }
      }
      break;
    case MicrostreamProtocol::HEARTBEAT:
      break;
    case MicrostreamProtocol::CONFIG:
      break;
    default:
      break;
  }
}
