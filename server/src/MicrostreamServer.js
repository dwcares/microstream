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
 * Protocol format (TCP with length prefix):
 *   [type (1 byte)][length (2 bytes LE)][payload (length bytes)]
 *
 * - AUDIO_DATA (0x01): followed by length + audio samples
 * - AUDIO_END (0x02): length = 0
 * - HEARTBEAT (0x03): length = 0
 * - CONFIG (0x04): followed by length + JSON payload
 * - ERROR (0x05): followed by length + UTF-8 payload
 */
class TcpSocketAdapter extends EventEmitter {
  constructor (socket) {
    super()
    this.OPEN = 1
    this._socket = socket
    this._readyState = 1
    this._buffer = Buffer.alloc(0) // Accumulate incoming data
    this._audioChunks = [] // Accumulate audio for session

    socket.on('data', (data) => {
      this._buffer = Buffer.concat([this._buffer, data])
      this._parseMessages()
    })

    socket.on('close', () => {
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

  _parseMessages () {
    // Parse complete messages from buffer
    while (this._buffer.length >= 3) {
      const type = this._buffer[0]
      const length = this._buffer.readUInt16LE(1)

      // Check if we have the complete message
      if (this._buffer.length < 3 + length) {
        break // Wait for more data
      }

      // Extract payload
      const payload = this._buffer.slice(3, 3 + length)

      // Emit as WebSocket-style message: [type][payload]
      const msg = Buffer.alloc(1 + length)
      msg[0] = type
      if (length > 0) {
        payload.copy(msg, 1)
      }
      this.emit('message', msg, true)

      // Remove processed message from buffer
      this._buffer = this._buffer.slice(3 + length)
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
