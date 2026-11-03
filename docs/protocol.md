# DSH connection contract

WhaleTalk Passport implements the GCP 7.0.0 audio subset used by the DSH
AI Passport connector. It is not a general Qwen Gateway implementation.

## Transport and identity

- HTTP health: `/api/health`; WebSocket: `/api/realtime`, default TCP 3101.
- `Authorization: Bearer <current code>`, exactly matching the Host setting.
  Short numeric pairing is restricted by the Host to private LAN/loopback peers.
- Browser-origin WebSocket requests are rejected by the connector.
- `session.hello` advertises min/max `7.0.0`, `input.audio`, `playback.receipts`.
- Keep `client.instance_id = bean_<Wi-Fi MAC hex>` and the existing client type
  for compatibility. Product branding does not rotate device identity.
- Device configuration remains in the `dsh_voice` NVS namespace.

## Audio and ordering

The Host negotiates `session.ready`, `voice.ready`, then `voice.state`.
Microphone input is PCM16LE mono 16,000 Hz in base64
`input_audio_buffer.append` messages. The Host performs segmentation and ASR.
Mute/unmute, wake and `response.cancel` are control events.

The Host produces the complete reply WAV and converts to PCM16LE mono 24,000 Hz.
`response.started` defines `responseId`; matching `audio.delta` messages carry
`sampleRate` and base64 PCM, followed by `audio.done`. The device fences stale
responses and acknowledges `playback.started`, `playback.ended` or
`playback.cancelled`. The Host waits for speaker completion before confirming
delivery and allowing another voice turn. Merely sending the bytes is not success.

The current Host uses 960-byte / 20ms wire chunks with bounded ahead-of-playback
scheduling. Fixed `await delay(20)` pacing can systematically underfeed audio on
coarse Windows timers. The Host must compensate scheduling drift and limit
catch-up bursts to the device's finite queue; changing only firmware does not
repair that Host behavior. Read the bundled debugging skill for measured evidence.

WhaleTalk keeps the reference partition layout and Recovery hook. This protocol
does not carry arbitrary Host credentials, route devices into unrelated sessions,
or grant automatic permission for Agent tools.
