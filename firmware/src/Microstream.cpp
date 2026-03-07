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
  if (_config.bitDepth == 0) _config.bitDepth = 8;
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
  // Only check the active playing flag, not buffered data
  // This ensures we properly transition out of playing state
  return _playback.isPlaying();
}

bool Microstream::isConnected() const {
  return _connected;
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

  while (buf.getSize() >= MIN_SEND_SIZE) {
    unsigned int size = min((unsigned int)buf.getSize(), MAX_SEND_SIZE);

    // TCP protocol: [type (1 byte)][length (2 bytes LE)][payload]
    _txBuffer[0] = MicrostreamProtocol::AUDIO_DATA;
    _txBuffer[1] = (uint8_t)(size & 0xFF);        // Length low byte
    _txBuffer[2] = (uint8_t)((size >> 8) & 0xFF); // Length high byte
    for (unsigned int i = 0; i < size; i++) {
      _txBuffer[3 + i] = buf.get();
    }

    _tcpClient.write(_txBuffer, 3 + size);
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
  // Raw TCP has no framing, so we use a heuristic to find message boundaries:
  // known message type bytes (0x01-0x05) are near-minimum values in 8-bit
  // unsigned PCM (centered at 128) and extremely rare in real audio.
  while (_tcpClient.available()) {
    uint8_t typeByte = _tcpClient.read();

    switch (typeByte) {
      case MicrostreamProtocol::AUDIO_DATA: {
        while (_tcpClient.available()) {
          int next = _tcpClient.peek();
          if (next >= MicrostreamProtocol::AUDIO_DATA &&
              next <= MicrostreamProtocol::MSG_ERROR) {
            break;
          }
          _playback.feed((uint8_t)_tcpClient.read());
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
        _playback.feed(data + 1, len - 1);
      }
      break;
    case MicrostreamProtocol::HEARTBEAT:
      // Heartbeat received
      break;
    case MicrostreamProtocol::CONFIG:
      // Could parse config JSON here in the future
      break;
    default:
      break;
  }
}
