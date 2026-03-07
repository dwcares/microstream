require('dotenv').config()

const fs = require('fs')
const path = require('path')
const { MicrostreamServer } = require('microstream-server')
const SpeechPipeline = require('./SpeechPipeline')

// Directory to save audio files
const AUDIO_DIR = path.join(__dirname, 'audio')
if (!fs.existsSync(AUDIO_DIR)) {
  fs.mkdirSync(AUDIO_DIR, { recursive: true })
}

const PORT = process.env.PORT || 5000

const server = new MicrostreamServer({
  port: PORT,
  audio: { sampleRate: 8000, bitDepth: 16, channels: 1 }
})

const pipeline = new SpeechPipeline({
  apiKey: process.env.OPENAI_API_KEY,
  model: process.env.OPENAI_MODEL || 'gpt-4o',
  voice: process.env.OPENAI_VOICE || 'alloy',
  systemPrompt: process.env.SYSTEM_PROMPT || undefined
})

server.on('session', (session) => {
  const tag = session.id.slice(0, 8)
  console.log(`[${tag}] Device connected`)

  pipeline.initSession(session.id)

  session.on('audioStart', () => {
    console.log(`[${tag}] Recording started`)
  })

  session.on('audioEnd', async (wavBuffer) => {
    console.log(`[${tag}] Recording ended (${wavBuffer.length} bytes)`)

    // Save incoming audio to file for playback on PC
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-')
    const filename = `recording-${tag}-${timestamp}.wav`
    const filepath = path.join(AUDIO_DIR, filename)
    fs.writeFileSync(filepath, wavBuffer)
    console.log(`[${tag}] Saved audio to: ${filepath}`)

    try {
      const result = await pipeline.processAudio(session.id, wavBuffer)
      if (result && result.audio) {
        session.play(result.audio)
      }
    } catch (err) {
      console.error(`[${tag}] Pipeline error:`, err.message)
    }
  })

  session.on('disconnect', () => {
    console.log(`[${tag}] Device disconnected`)
    pipeline.cleanupSession(session.id)
  })

  session.on('error', (err) => {
    console.error(`[${tag}] Session error:`, err.message)
  })
})

server.on('error', (err) => {
  console.error('Server error:', err.message)
})

server.on('listening', (port) => {
  console.log(`Microstream server listening on port ${port}`)
  console.log(`Waiting for device connections...`)
})

server.listen()
