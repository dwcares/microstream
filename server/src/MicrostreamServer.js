const { EventEmitter } = require('events')
const net = require('net')
const http = require('http')
const { WebSocketServer } = require('ws')
const Session = require('./Session')
const { MessageType } = require('./Protocol')

/**
 * Wraps a raw TCP net.Socket to look like a WebSocket (ws),
 * so Session can work with both connection types.
 *
 * Parses the raw TCP stream into discrete protocol messages.
 *
 * Protocol format: Each message is [type (1 byte) | payload (0-N bytes)]
 * - AUDIO_DATA (0x01): followed by audio samples until next message type
 * - AUDIO_END (0x02): no payload, signals end of recording
 * - HEARTBEAT (0x03): no payload
 * - CONFIG (0x04): followed by JSON payload
 * - ERROR (0x05): followed by UTF-8 payload
 *
 * The firmware sends each message as a separate TCP write, but TCP may
 * coalesce them. We detect message boundaries by looking for message type
 * bytes at the START of each TCP chunk (where write boundaries likely are).
 */
class TcpSocketAdapter extends EventEmitter {
  constructor (socket) {
    super()
    this.OPEN = 1
    this._socket = socket
    this._readyState = 1
    this._audioBuffer = [] // Accumulate audio samples
    this._inAudioMessage = false

    socket.on('data', (data) => {
      this._parseStream(data)
    })

    socket.on('close', () => {
      this._flushAudio()
      this._readyState = 3
      this.emit('close')
    })

    socket.on('error', (err) => {
      this._readyState = 3
      this.emit('error', err)
    })
  }

  get readyState () {
    return this._readyState
  }

  send (data, options) {
    if (this._readyState === 1) {
      this._socket.write(Buffer.from(data))
    }
  }

  close () {
    this._readyState = 3
    this._socket.end()
  }

  _flushAudio () {
    if (this._audioBuffer.length > 0) {
      const msg = Buffer.alloc(1 + this._audioBuffer.length)
      msg.writeUInt8(MessageType.AUDIO_DATA, 0)
      for (let i = 0; i < this._audioBuffer.length; i++) {
        msg.writeUInt8(this._audioBuffer[i], 1 + i)
      }
      this.emit('message', msg, true)
      this._audioBuffer = []
    }
  }

  _emitSimpleMessage (type) {
    const msg = Buffer.alloc(1)
    msg.writeUInt8(type, 0)
    this.emit('message', msg, true)
  }

  _parseStream (data) {
    let i = 0

    // Check if this chunk starts with a message type byte
    // This is where TCP write boundaries are most likely preserved
    if (data.length > 0) {
      const firstByte = data[0]

      if (firstByte === MessageType.AUDIO_END) {
        // End of recording - flush any buffered audio first
        this._flushAudio()
        this._emitSimpleMessage(MessageType.AUDIO_END)
        this._inAudioMessage = false
        i = 1 // Skip this byte
      } else if (firstByte === MessageType.HEARTBEAT) {
        this._emitSimpleMessage(MessageType.HEARTBEAT)
        i = 1
      } else if (firstByte === MessageType.AUDIO_DATA) {
        // New audio message starting
        this._inAudioMessage = true
        i = 1 // Skip the type byte, rest is audio data
      }
      // Other bytes at start: treat as continuation of previous message
    }

    // Process remaining bytes as audio data
    for (; i < data.length; i++) {
      const byte = data[i]

      // Check for embedded AUDIO_END (can happen if TCP coalesced writes)
      // Only treat as AUDIO_END if it's a standalone 0x02 followed by
      // either end of chunk or another message type
      if (byte === MessageType.AUDIO_END) {
        const nextByte = i + 1 < data.length ? data[i + 1] : -1
        // If next byte is a message type or end of data, this is likely real AUDIO_END
        if (nextByte === -1 || nextByte === MessageType.AUDIO_DATA ||
            nextByte === MessageType.HEARTBEAT || nextByte === MessageType.CONFIG) {
          this._flushAudio()
          this._emitSimpleMessage(MessageType.AUDIO_END)
          this._inAudioMessage = false
          continue
        }
      }

      // Check for new AUDIO_DATA message start
      if (byte === MessageType.AUDIO_DATA) {
        const nextByte = i + 1 < data.length ? data[i + 1] : -1
        // If followed by typical audio values (> 5), this is likely a new message
        if (nextByte > MessageType.ERROR) {
          // This is a new AUDIO_DATA message, skip the type byte
          this._inAudioMessage = true
          continue
        }
      }

      // Accumulate as audio data
      this._audioBuffer.push(byte)
    }
  }
}

class MicrostreamServer extends EventEmitter {
  constructor (options = {}) {
    super()

    this._port = options.port || 5000
    this._audioConfig = Object.assign(
      { sampleRate: 16000, bitDepth: 8, channels: 1 },
      options.audio
    )
    this._sessions = new Map()
    this._server = null
    this._httpServer = null
    this._wss = null
  }

  get sessions () {
    return this._sessions
  }

  get port () {
    return this._port
  }

  listen (callback) {
    this._httpServer = http.createServer()

    this._wss = new WebSocketServer({ server: this._httpServer })

    this._wss.on('connection', (ws) => {
      this._createSession(ws, 'ws')
    })

    this._wss.on('error', (err) => {
      this.emit('error', err)
    })

    // Raw TCP server that detects protocol on first bytes
    this._server = net.createServer((socket) => {
      socket.once('readable', () => {
        const chunk = socket.read()
        if (!chunk || chunk.length === 0) return

        // WebSocket upgrade starts with "GET "
        if (chunk.length >= 4 && chunk.toString('utf8', 0, 4) === 'GET ') {
          // It's HTTP/WebSocket — hand off to the HTTP server
          socket.unshift(chunk)
          this._httpServer.emit('connection', socket)
        } else {
          // Raw TCP — wrap in adapter and create session
          socket.unshift(chunk)
          const adapter = new TcpSocketAdapter(socket)
          this._createSession(adapter, 'tcp')
        }
      })
    })

    this._server.on('error', (err) => {
      this.emit('error', err)
    })

    this._server.listen(this._port, () => {
      this.emit('listening', this._port)
      if (callback) callback()
    })
  }

  _createSession (ws, type) {
    const session = new Session(ws, this._audioConfig)
    this._sessions.set(session.id, session)

    session.on('disconnect', () => {
      this._sessions.delete(session.id)
    })

    this.emit('session', session)
  }

  close (callback) {
    for (const session of this._sessions.values()) {
      session.close()
    }
    this._sessions.clear()

    let pending = 0
    const done = () => {
      pending--
      if (pending <= 0 && callback) callback()
    }

    if (this._wss) {
      pending++
      this._wss.close(done)
      this._wss = null
    }

    if (this._server) {
      pending++
      this._server.close(done)
      this._server = null
    }

    if (this._httpServer) {
      pending++
      this._httpServer.close(done)
      this._httpServer = null
    }

    if (pending === 0 && callback) callback()
  }
}

module.exports = MicrostreamServer
