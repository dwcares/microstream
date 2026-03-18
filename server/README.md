# microstream-server

Bidirectional audio streaming server for Particle microcontrollers (Photon, Argon). Accepts connections over TCP or WebSocket and handles real-time PCM audio streaming with automatic WAV conversion.

## Installation

```bash
npm install microstream-server
```

## Quick Start

```javascript
const { MicrostreamServer } = require('microstream-server')

const server = new MicrostreamServer({
  port: 5000,
  audio: { sampleRate: 16000, bitDepth: 16, channels: 1 }
})

server.on('session', (session) => {
  console.log(`Device connected: ${session.id}`)

  session.on('audioEnd', (wavBuffer) => {
    console.log(`Received ${wavBuffer.length} bytes of audio`)

    // Process audio here (speech-to-text, etc.)
    // Then send response back:
    const responseAudio = processAudio(wavBuffer)
    session.play(responseAudio)
  })

  session.on('disconnect', () => {
    console.log(`Device disconnected: ${session.id}`)
  })
})

server.listen(() => {
  console.log('Microstream server listening on port 5000')
})
```

## API Reference

### MicrostreamServer

Main server class that accepts device connections.

```javascript
const server = new MicrostreamServer(options)
```

#### Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `port` | `number` | `5000` | Port to listen on |
| `audio.sampleRate` | `number` | `16000` | Audio sample rate in Hz |
| `audio.bitDepth` | `number` | `8` | Bits per sample (8 or 16) |
| `audio.channels` | `number` | `1` | Number of audio channels |

#### Methods

| Method | Description |
|--------|-------------|
| `listen(callback)` | Start accepting connections |
| `close(callback)` | Stop server and disconnect all sessions |

#### Properties

| Property | Type | Description |
|----------|------|-------------|
| `sessions` | `Map` | Active sessions by ID |
| `port` | `number` | Configured port number |

#### Events

| Event | Payload | Description |
|-------|---------|-------------|
| `listening` | `port` | Server started |
| `session` | `Session` | New device connected |
| `error` | `Error` | Server error |

### Session

Represents a connected device.

#### Properties

| Property | Type | Description |
|----------|------|-------------|
| `id` | `string` | Unique session identifier (UUID) |
| `state` | `string` | Current state: `'idle'` or `'recording'` |
| `connected` | `boolean` | Connection status |

#### Methods

| Method | Description |
|--------|-------------|
| `play(pcmBuffer)` | Send audio to device for playback |
| `close()` | Disconnect the session |

#### Events

| Event | Payload | Description |
|-------|---------|-------------|
| `audioStart` | - | Device started recording |
| `audioData` | `Buffer` | Raw PCM chunk received |
| `audioEnd` | `Buffer` | Complete recording as WAV |
| `disconnect` | - | Device disconnected |
| `error` | `Error` | Session error |

### Protocol

Binary message utilities for the streaming protocol.

```javascript
const { Protocol } = require('microstream-server')

// Message types
Protocol.MessageType.AUDIO_DATA  // 0x01
Protocol.MessageType.AUDIO_END   // 0x02
Protocol.MessageType.HEARTBEAT   // 0x03
Protocol.MessageType.CONFIG      // 0x04
Protocol.MessageType.ERROR       // 0x05

// Encode/decode
const msg = Protocol.decode(buffer)
const data = Protocol.encodeAudioData(pcmChunk)
```

### AudioBuffer

Accumulates PCM chunks and produces WAV files.

```javascript
const { AudioBuffer } = require('microstream-server')

const buffer = new AudioBuffer({ sampleRate: 16000, bitDepth: 16, channels: 1 })
buffer.append(chunk1)
buffer.append(chunk2)

const wav = buffer.toWav()    // WAV with header
const pcm = buffer.toPcm()    // Raw PCM
buffer.clear()

// Static helper
const wav = AudioBuffer.wrapPcmAsWav(pcmBuffer, { sampleRate: 16000, bitDepth: 16, channels: 1 })
```

## Protocol Format

Messages use a simple binary format:

**WebSocket:** `[type (1 byte)][payload (0-N bytes)]`

**TCP:** `[type (1 byte)][length (2 bytes LE)][payload (0-N bytes)]`

The server auto-detects WebSocket vs raw TCP connections on the same port.

## Firmware

Use the companion Particle library for device firmware:

```
particle library add microstream
```

See [microstream on Particle](https://build.particle.io/libs/microstream) for firmware documentation.

## Examples

### Voice Assistant

```javascript
const { MicrostreamServer } = require('microstream-server')
const OpenAI = require('openai')

const server = new MicrostreamServer({ port: 5000 })
const openai = new OpenAI()

server.on('session', (session) => {
  session.on('audioEnd', async (wavBuffer) => {
    // Transcribe
    const transcript = await openai.audio.transcriptions.create({
      model: 'whisper-1',
      file: wavBuffer
    })

    // Generate response
    const completion = await openai.chat.completions.create({
      model: 'gpt-4',
      messages: [{ role: 'user', content: transcript.text }]
    })

    // Synthesize and send back
    const speech = await openai.audio.speech.create({
      model: 'tts-1',
      voice: 'alloy',
      input: completion.choices[0].message.content,
      response_format: 'pcm'
    })

    session.play(Buffer.from(await speech.arrayBuffer()))
  })
})

server.listen()
```

### Echo Server

```javascript
const { MicrostreamServer } = require('microstream-server')

const server = new MicrostreamServer({ port: 5000 })

server.on('session', (session) => {
  session.on('audioEnd', (wavBuffer) => {
    // Strip WAV header and echo back raw PCM
    const pcm = wavBuffer.slice(44)
    session.play(pcm)
  })
})

server.listen()
```

## License

MIT License - see [LICENSE](LICENSE)
