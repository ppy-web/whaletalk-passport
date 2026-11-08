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

After final reply text is ready, supported Host providers stream PCM16LE mono 24,000 Hz; other models convert the complete WAV.
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

## Local appearance portal

These HTTP routes exist only in the device's provisioning hotspot and are
independent of the DSH audio protocol:

- `GET /appearance` opens background selection/upload. `GET /current` adds
  `background` (0 = Seaside Tea, 1 = Moonlit Dream, 2 = custom) and
  `customBackground` (a usable custom image is present).
- `POST /background/upload` requires the current `X-Setup-Nonce` and exactly
  6,184 binary bytes. WTBG v1 starts with `57 54 42 47 01 60 80 10`, followed
  by sixteen RGB565LE palette entries and 6,144 I4 pixel bytes, high nibble
  first. Dimensions are fixed at 96×128. Invalid length/version/header,
  interrupted uploads and storage errors never trigger a reboot or erase
  connection settings. NVS stores the bounded blob under `dsh_voice/bg_custom`.
- `POST /background/apply` requires the same nonce and JSON
  `{"background":0|1|2}`. Custom selection requires a usable or newly uploaded
  image. A successful preference save schedules a reboot; saved Wi-Fi,
  pairing, endpoint and device identity are reused. No active credentials
  are required in this request.

Background and brightness preferences are additional NVS keys, not a config
blob or partition migration. `brightness` accepts 25, 50, 75 or 100; missing
or invalid values default to 100. Backlight sleep uses 0 without overwriting
the preference. Thinking animation is local presentation; it neither changes
`voice.state`, acknowledges audio early, nor shortens Host processing time.

## Optional voice feedback

The device advertises `voice.feedback`; the connector echoes it only when supported.
`voice.progress` carries `turnId` and `phase` (`capturing`, `recognizing`, `thinking`, `tool`, `synthesizing`, `playing`). Capturing does not pause the microphone. `voice.result` carries the same `turnId`, a status (`completed`, `cancelled`, `failed`) and an optional stable `code`. Duplicate terminal results and stale progress for settled turns are ignored.

Failure explanations persist across ordinary listening updates until new valid input, a user action or connection failure. Turn failures preserve the microphone and transport, invalidate old playback and schedule one local cue. Arbitrary server prose is never rendered. Fatal protocol/transport failures retain the reconnect path. Legacy recoverable `error` codes are handled locally without requiring new feedback events.

Streaming keeps one response ID through all chunks and waits for `playback.ended`. A partial synthesis failure or duration limit clears queued playback and reports failure, never successful delivery or automatic replay.
