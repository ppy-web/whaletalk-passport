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
- Connection profiles are stored in one versioned `profiles` blob (up to three complete
  version-one configurations, most recent/selected first). The original `config` blob
  remains untouched as the legacy read fallback when `profiles` is absent. A save updates
  by SSID and evicts the oldest entry at capacity; selecting a saved entry atomically
  moves its full endpoint and credentials to the front and reboots. Reads and rejected
  saves do not migrate or erase data. Downgrading to older firmware sees its original
  single configuration. Identity, session keys and partition layout are unchanged.

## Audio and ordering

The Host negotiates `session.ready`, `voice.ready`, then `voice.state`.
Microphone input is PCM16LE mono 16,000 Hz in base64
`input_audio_buffer.append` messages. The Host performs segmentation and ASR.
Mute/unmute, wake and `response.cancel` are control events.

Screen idle now pauses effective microphone input without changing the user's
mic preference. The device sends `input.mute` before any further PCM upload;
this clears partial capture through the connector's existing interruption path.
On wake with an open mic, `wake` precedes `input.unmute`, and both must be sent
successfully before uploading fresh frames. A closed mic remains closed.
Capture epochs invalidate queued pre-sleep/pre-wake frames. Reconnecting during
standby advertises `connection.input_enabled=false` and resynchronizes mute;
it retains the device identity and conversation. No new protocol capability or
Host connector change is required. Active turns and pending push-to-talk work
defer standby. Wi-Fi association and WebSocket keepalives remain active.

## Optional push-to-talk

The device advertises `input.push_to_talk`; it is enabled only when echoed by
`session.ready`. A held DOWN gesture cancels any previous turn, then sends
`input_audio_buffer.start` with an increasing decimal-string `captureId`.
The connector sends `voice.ptt.started` with that ID before accepting a final
`input_audio_buffer.commit` with the same ID. Audio uses the existing append
messages. Manual capture does not commit on silence. Empty capture returns
`voice.ptt.empty` with the ID; stale starts and commits cannot affect a newer
capture. Cancellation, disconnection, duration limits and queue overflow
discard unfinished audio. The normal microphone setting is not changed:
while it is muted, the manual response is still played and acknowledged.
The device buffers the pre-long-press audio locally and does not upload it for
a short cancellation press. Pre-roll and playback reuse one bounded 32-frame
queue. A failed or incompatible negotiation leaves ordinary controls intact.

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
  `background` (0 = Seaside Tea, 1 = Moonlit Dream, 2 = custom, 3 = off,
  4 = Spring Sakura, 5 = Summer Shore, 6 = Autumn Maple, 7 = Winter Snow) and
  `customBackground` (a usable custom image is present).
- `GET /background-season.bmp?id=4` serves the Spring Sakura preview as an
  indexed BMP. IDs 5–7 select the other seasons.
- `POST /background/upload` requires the current `X-Setup-Nonce` and exactly
  6,184 binary bytes. WTBG v1 starts with `57 54 42 47 01 60 80 10`, followed
  by sixteen RGB565LE palette entries and 6,144 I4 pixel bytes, high nibble
  first. Dimensions are fixed at 96×128. Invalid length/version/header,
  interrupted uploads and storage errors never trigger a reboot or erase
  connection settings. NVS stores the bounded blob under `dsh_voice/bg_custom`.
- `POST /background/apply` requires the same nonce and a JSON object with
  an integer `background` from 0 through 7. Custom selection requires a usable or newly uploaded
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

Supported progress phases map to local labels in the home screen's top status line.
Actionable failure, long-wait and push-to-talk messages take priority over progress;
long messages scroll horizontally. No server-provided prose is displayed.

Failure explanations persist across ordinary listening updates until new valid input, a user action or connection failure. Turn failures preserve the microphone and transport, invalidate old playback and schedule one local cue. Arbitrary server prose is never rendered. Fatal protocol/transport failures retain the reconnect path. Legacy recoverable `error` codes are handled locally without requiring new feedback events.

Streaming keeps one response ID through all chunks and waits for `playback.ended`. A partial synthesis failure or duration limit clears queued playback and reports failure, never successful delivery or automatic replay.
