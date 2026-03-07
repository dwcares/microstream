const OpenAI = require('openai')
const { toFile } = require('openai')

class SpeechPipeline {
  constructor (options = {}) {
    this._openai = new OpenAI({ apiKey: options.apiKey || process.env.OPENAI_API_KEY })
    this._model = options.model || 'gpt-4o'
    this._voice = options.voice || 'alloy'
    this._systemPrompt = options.systemPrompt || 'You are a helpful voice assistant running on a small robot. Keep responses concise — under 2 sentences.'

    // Per-session conversation history
    this._conversations = new Map()
  }

  initSession (sessionId) {
    this._conversations.set(sessionId, [
      { role: 'system', content: this._systemPrompt }
    ])
  }

  cleanupSession (sessionId) {
    this._conversations.delete(sessionId)
  }

  async processAudio (sessionId, wavBuffer) {
    // 1. Speech-to-text via Whisper
    const transcript = await this._transcribe(wavBuffer)
    if (!transcript) return null

    console.log(`[${sessionId.slice(0, 8)}] User: ${transcript}`)

    // 2. Chat completion via GPT
    const response = await this._chat(sessionId, transcript)
    console.log(`[${sessionId.slice(0, 8)}] Assistant: ${response}`)

    // 3. Text-to-speech → convert to device format
    const ttsBuffer = await this._synthesize(response)
    const deviceAudio = SpeechPipeline.convertTtsToDevice(ttsBuffer)

    console.log(`[${sessionId.slice(0, 8)}] Audio: ${ttsBuffer.length}b TTS → ${deviceAudio.length}b device`)

    return { transcript, response, audio: deviceAudio }
  }

  async _transcribe (wavBuffer) {
    // Use OpenAI's toFile helper to create a proper file object from buffer
    const file = await toFile(wavBuffer, 'recording.wav', { type: 'audio/wav' })

    const result = await this._openai.audio.transcriptions.create({
      model: 'whisper-1',
      file: file,
      response_format: 'text'
    })
    return result.trim()
  }

  async _chat (sessionId, userMessage) {
    const history = this._conversations.get(sessionId)
    if (!history) return 'Sorry, session not found.'

    history.push({ role: 'user', content: userMessage })

    const completion = await this._openai.chat.completions.create({
      model: this._model,
      messages: history
    })

    const response = completion.choices[0].message.content
    history.push({ role: 'assistant', content: response })

    // Keep conversation history manageable (last 20 exchanges)
    if (history.length > 41) { // system + 20 pairs
      history.splice(1, 2) // remove oldest user/assistant pair
    }

    return response
  }

  async _synthesize (text) {
    const response = await this._openai.audio.speech.create({
      model: 'tts-1',
      voice: this._voice,
      input: text,
      response_format: 'pcm' // raw 24kHz 16-bit signed LE mono
    })

    const arrayBuffer = await response.arrayBuffer()
    return Buffer.from(arrayBuffer)
  }

  /**
   * Convert OpenAI TTS output to device format.
   *
   * TTS returns:  24kHz, 16-bit signed LE, mono
   * Device needs:  16kHz, 16-bit signed LE, mono
   */
  static convertTtsToDevice (ttsBuffer) {
    const inputSamples = ttsBuffer.length / 2 // 2 bytes per 16-bit sample
    const outputSamples = Math.floor(inputSamples * 16000 / 24000)
    const output = Buffer.alloc(outputSamples * 2) // 2 bytes per 16-bit sample
    const ratio = 24000 / 16000 // 1.5

    // Fade in/out duration in output samples (10ms at 16kHz = 160 samples)
    const fadeSamples = 160

    for (let i = 0; i < outputSamples; i++) {
      // Linear interpolation for smoother resampling
      const srcPos = i * ratio
      const srcIndex = Math.floor(srcPos)
      const frac = srcPos - srcIndex

      // Get two adjacent samples for interpolation
      const sample1 = srcIndex < inputSamples ? ttsBuffer.readInt16LE(srcIndex * 2) : 0
      const sample2 = srcIndex + 1 < inputSamples ? ttsBuffer.readInt16LE((srcIndex + 1) * 2) : sample1

      // Interpolate between samples
      let sample16 = Math.round(sample1 + frac * (sample2 - sample1))

      // Apply fade in/out to prevent clicks at audio boundaries
      if (i < fadeSamples) {
        // Fade in
        sample16 = Math.round(sample16 * (i / fadeSamples))
      } else if (i >= outputSamples - fadeSamples) {
        // Fade out
        const remaining = outputSamples - i
        sample16 = Math.round(sample16 * (remaining / fadeSamples))
      }

      // Clamp to 16-bit signed range
      sample16 = Math.max(-32768, Math.min(32767, sample16))

      // Write as 16-bit signed LE
      output.writeInt16LE(sample16, i * 2)
    }

    return output
  }
}

module.exports = SpeechPipeline
