English · [简体中文](guide.zh_CN.md)

# 鲸语 · WhaleTalk Passport

AI Passport becomes a voice terminal for the local DSH instance:
microphone → Wi-Fi → plugin Passport adapter → selected Host/Xiaomi/Doubao ASR → dedicated Agent
conversation → plugin TTS → device speaker. No Qwen Audio Agent backend is
required. The computer must keep DSH running.

## Hardware and interface

ESP32-C3, 8 MB Flash, no PSRAM, 240×320 screen, ES8311 audio and three buttons.
The app uses the official BSP and adapts Qwen Voice Bean's expressive face and
conversation flow. Its on-device name is 鲸语. It defaults to Whale Girl, with
the original mint face available as a second appearance. Both share setup,
connection, ready, listening, thinking, speaking, error, settings and confirmed
reconfiguration screens. Main labels and states use 16px fonts and the provisioning address uses 20px. Chinese labels use compiled font subsets; arbitrary
model-response transcripts are not displayed.

The home title is centered. To its left, joined/broken chain links show the DSH
connection, followed by a microphone icon with a slash when disabled. An enabled
microphone does not imply audio is currently uploading. The battery at the right
fills with its measured charge, turns red at 15% or below, and shows a dash when
unknown. Normal listening, thinking and speech use a waveform, three dots and a
speaker alongside the character; connection and error details remain readable.
The three bottom groups map UP to volume, OK to the microphone, and DOWN to
cancel. Five volume bars represent 25, 50, 75, 90 and 100%; a crossed speaker is
muted. Each gesture hint has a rounded border and at most two icons; volume bars
are integrated into the speaker indicator. The home screen omits the hold-OK
hint; holding OK still opens settings.

| Button | Home action |
| --- | --- |
| Short OK | Toggle microphone; initially closed |
| Short UP | Cycle 0, 25, 50, 75, 90, 100%; 0 mutes |
| Short DOWN | Cancel recognition, generation or playback |
| Hold DOWN | Speak while holding; release to send |
| Hold OK | Open settings; hold again to return home |

Push-to-talk works alongside continuous microphone mode and restores the prior
microphone setting afterwards. Holding DOWN interrupts a current response. With
audible cues enabled and volume above zero, wait for the start tone to finish and
the recording indication before speaking; releasing before readiness discards the
recording. The cue and its acoustic tail are not uploaded. With cues disabled or
muted, local pre-roll is retained; a short cancellation press never uploads it.
A connector supporting push-to-talk is required; older connectors retain short-press
cancellation. Turning interface hints off also hides the recording indication.

Settings groups appearance and screen controls under System settings (系统设置); select 返回设置 to go back, or hold OK to return home. Interface hints (界面提示) default to on. When off, the home screen shows only the background and character; title, status and button icons and messages are hidden, but the physical buttons still work. Provisioning remains visible. The preference survives restart; a save failure is shown in settings and toggling retries it. Home messages share a rounded, translucent panel and scroll when long without moving the bottom controls.

Sound cues (提示音) in System settings default to on and survive restart.
Turning them off disables both operation and failure cues without changing spoken
replies. Cues follow the device volume, including mute. Microphone changes,
push-to-talk, cancellation, connection changes, failures and volume adjustments
use short tones; moving through settings remains silent. Cues that would
interfere with speech or recording may be dropped. Toggle again to retry a failed save.
The device synthesizes these tones without external audio assets; the feedback
choices draw on [Apple's feedback guidance](https://developer.apple.com/design/human-interface-guidelines/feedback)
and [Microsoft's sound design guidance](https://learn.microsoft.com/en-us/windows/mixed-reality/design/spatial-sound-design).

Status information (状态信息) shows the configured Wi-Fi, computer address and saved-password/pairing status without displaying credentials. The operation guide continues below the status values and covers home controls, settings, network setup and background upload. UP/DOWN scroll and OK returns to settings.

Thinking cycles through thoughtful, attentive and blinking expressions every
3.2 seconds, alongside bouncing dots. It remains in the real processing phase
and DOWN still cancels. The animation itself does not shorten recognition, model or TTS latency.

In System settings (系统设置), Background (背景) cycles Seaside Tea, Moonlit Dream and an available
custom background, and Off (关闭). Off shows the character against the theme color and retains the uploaded image. Selection applies immediately, survives restart and is saved
after thinking/speech finishes. Failed saves can be retried. The System settings list
scrolls with UP/DOWN.
Select 上传背景 to restart into the setup hotspot, interrupting voice while keeping
saved connection information. Connect a phone to `DSH-Voice-*`, open
`http://192.168.4.1`, then select the top 自定义背景 tab. The 连接鲸语 tab returns to network setup. Choose a built-in, turn the background off, or upload JPG, PNG
or WebP (up to 8 MB and 24 megapixels). The browser center-crops to 3:4 and
quantizes to 96×128 with 16 colors, showing the converted result. Uploads are
exactly 6,184 bytes. Upload first, then save/restart; no Wi-Fi password or pairing
code is needed again. The device retains the encoded data and renders with a
480-byte scanline rather than expanding/scaling a full RGB565 image. The existing
`dsh_voice` NVS namespace stores the image;
partitions, device identity and connection settings are preserved. Invalid data
or allocation failures fall back to a built-in without erasing storage.

Download eight sample PNGs from the [background gallery and upload instructions](background-examples.md)
(Chinese): mint, a Whale Girl lagoon, night, four seasons and a rainy reading nook.
Files live in `assets/backgrounds/examples/` and are examples for manual upload;
they are not added to the built-in choices. Only the most recently uploaded custom image is retained.

In System settings, Brightness (亮度) cycles 25%, 50%, 75% and 100% backlight brightness.
Changes apply immediately and persist across restarts; the default is 100%.
Sleeping still sets 0%; a key wake or thinking/speech restores the chosen level.
Save errors appear in settings; cycle again to retry. Flash writes wait until
thinking/speech finishes.

In System settings, select Sleep (息屏) and briefly press OK to cycle through
1 minute, 3 minutes, 10 minutes and Never. Changes apply immediately and are
saved across restarts; the default is 1 minute. A failed save is shown in
settings; cycle again to retry. Saving is deferred while thinking or speaking.
After the selected idle period, the backlight and face animation turn off.
Never keeps the screen on.
Networking and an enabled microphone continue working. The first button gesture
only wakes the screen; press again for its normal action. Thinking or speaking
automatically wakes the screen and keeps it on, restarting the idle countdown.
Provisioning and unavailable buttons keep the screen on for access and diagnosis.
This is backlight sleep rather than device deep sleep. Networking and capture
still use power; battery-life improvement needs measurement on hardware.

In settings, UP/DOWN select and OK confirms. Reconfiguration needs confirmation;
cancelling keeps existing settings. Audio uploads only after the user enables
the microphone. Doubao recognition uploads while capture continues. Capture pauses after local segmentation while waiting for final recognition/thinking/playback, and resumes after
speaker completion is acknowledged. No automatic voice interruption or acoustic
echo cancellation is provided. Handle tool approvals in DSH.

Hold OK, open System settings (系统设置), select Interface (界面), and briefly press OK to switch
between Whale Girl and the mint face. Hold OK again to return home. The choice
survives restart without changing Wi-Fi, pairing or the conversation. Saving is
deferred during speech/thinking, and a failed save is shown in settings. Whale
Girl has six expression frames; see [asset provenance and build details](whale-girl-assets.md).

## Development and builds

Use Python 3.10+ and ESP-IDF **5.5.3**. Node.js/npx is needed only when regenerating fonts. In PowerShell from this
application directory:

```powershell
./tools/setup.ps1
. ./tools/activate.ps1
python tools/validate.py --firmware
```

The scripts install IDF and its tools into `.tools/esp-idf-v5.5.3` and
`.tools/espressif` inside this repository, without editing global PATH or
the DSH profile. Pass `-IdfPath`, `-ToolsPath` and `-HostToolsPath` to reuse existing installations; use the
same arguments for activation. Other IDF installations remain intact. For the
official mainland China download route, set terminal-local
`IDF_GITHUB_ASSETS=dl.espressif.cn/github_assets` and
`IDF_COMPONENT_STORAGE_URL=https://components-file.espressif.cn`.

Setup also installs an isolated Zig C compiler and Pillow in the repository's
`.tools/host`. Activation adds the compiler only to the current
terminal PATH. Use that environment's Python with `tools/render_preview.py`
to convert LVGL preview frames to PNG and an overview image.

The first build downloads components pinned in `dependencies.lock`. Do not edit
generated `build/` or `managed_components/`. The firmware gate uses isolated
`build/firmware-build/sdkconfig` from tracked `sdkconfig.defaults`.

```powershell
python tools/validate.py --static
python tools/validate.py --preview
python tools/validate.py
```

Host tests need a native C compiler: `CC` naming an executable, or `cc`, `gcc`,
or `zig` on PATH. UI preview also needs CMake, Ninja and the downloaded LVGL
component. It renders the actual application UI with synthetic states, checks
visible text/glyphs/bounds/controls and writes `build/preview/*.ppm`. These are
host renders, not device captures or hardware acceptance.

After changing Chinese labels, run `python tools/generate_online_font.py`
(Node.js/npx required). The default font source comes from downloaded LVGL;
override `ONLINE_FONT_SOURCE` with an appropriately licensed full font if
needed. Run `--check` afterward to verify coverage.

The Host connector and pacing compensation are maintained in the separate DSH plugin.
Run its pnpm/WebSocket tests in that repository. This repository independently runs
C state tests, the firmware gate, LVGL preview and diagnostic helper tests.

## Connect to local DSH

1. Select and test Host ASR (such as SenseVoice) or Xiaomi/Doubao cloud recognition, the default conversation model and plugin TTS.
The default Agent preset is the plugin’s `whale-girl` (enable the Whale Girl preset component first). The default workspace is `~/.dsh-xiaomi-tts/passport/workspace`, created before a new conversation. Existing conversations keep their preset and directory.

To change presets, choose one from the updated DSH connector's conversation list and click
“Apply preset and start a new conversation”. The device reconnects automatically, retaining old
conversation history and existing Wi-Fi/pairing settings. Normal saves and reconnects retain the
current conversation. IDs may contain Chinese; use the Host roster and reload it after enabling
a new preset. The home status bar uses 70% background opacity; messages and outlined key hints
use 60%, allowing the picture to show through while keeping controls legible.

2. Enable AI Passport Connector and **generate a pairing code and enable the connection**. It displays available computer IPs and **four digits**, and listens on `0.0.0.0:3101`. Enter the computer LAN IP on the device, never `0.0.0.0`, `127.0.0.1` or the device address.
3. Join the displayed `DSH-Voice-*` hotspot on a phone; **no password is needed**. Open `http://192.168.4.1` and enter the computer IP plus the same four digits, including leading zeros.
4. First-time setup also needs 2.4 GHz Wi-Fi credentials. Keep saved Wi-Fi to change only the computer address or pairing code. Port 3101 is under Advanced settings.
   Use Wi-Fi on the computer's LAN or its 2.4 GHz mobile hotspot; enable it before entering setup. If missing, use **Refresh Wi-Fi list** to scan again, or enter a hidden network manually. Reflashing is unnecessary. The setup page may briefly slow down during scanning; wait for the results.
5. Saving restarts the device. When ready, briefly press OK, speak and pause for the complete reply. Hold OK to open settings and reconfigure.

Every mode uses four-digit pairing and admits only local/private LAN devices. Existing long credentials remain usable during migration; older firmware needs an update for short codes. The computer distinguishes listener, ASR and device readiness. The device distinguishes pairing mismatch, a disabled listener, wrong port and timeouts.

For a Windows computer mobile hotspot, use its adapter IP (usually `192.168.137.1`).
The connector lists both hotspot and ordinary LAN addresses. Web port 3080 is
independent of device port 3101. See the bundled
[connection skill](../.agents/skills/whaletalk-connect-debug/references/connection.md)
for bounded firewall rules and diagnosis.

Allow the listener through the computer firewall. On computers with multiple interfaces, choose an IP reachable from device Wi-Fi. A computer currently connected to the device setup hotspot must not mistake that temporary network for its regular LAN. The hotspot is for provisioning; DSH must keep running on the computer.

Wi-Fi credentials and pairing codes live in the unencrypted `dsh_voice` NVS namespace. The provisioning page never returns saved passwords or pairing codes. Recordings are not stored on the device. DSH manages conversation persistence and tool approvals; see the root [plugin handbook](https://github.com/ppy-web/dsh-plugin-xiaomi-mimo-tts/blob/main/handbook.md).

## Artifacts and hardware acceptance

Successful builds produce `build/WhaleTalk-Passport-full.bin`, a merged image for
**0x0**, and matching ELF/MAP/manifest under `build/firmware/<SHA-256>/`.

```powershell
python tools/archive_firmware.py verify build/firmware/<SHA-256>
```

The reference layout retains its 3 MB app partition, `cardid@0x356000`,
`recovery@0x700000` and five-second UP boot hook. The merged image does not
include a new Recovery image; the hook works only if compatible Recovery is
already installed. Merged flashing may reset NVS provisioning. An app-only image
must never be written at 0x0. Preserving settings requires confirming the
existing layout and using compatible segmented flashing.

Builds and USB enumeration do not flash. Confirm the exact device, artifact
hash, data impact and authorization before writing. Hardware checks must
separately cover Chinese rendering, buttons, actual ASR, audible TTS quality,
two consecutive turns, DOWN cancellation, mute and Wi-Fi reconnection. Passing
compilation or simulations does not establish those results.

See [NOTICE](../NOTICE) for sources and licenses. Development references:
[official tutorial](https://ai-passport.folotoy.cn/guides/create-a-play-with-agent/)
and [Qwen Voice Bean](https://github.com/liutaocode/esp32demo/tree/main/examples/qwen-voice-bean).

## Recognition and feedback

Choose Host, Xiaomi or Doubao recognition in the computer connector, independently of TTS. Xiaomi submits complete utterances; Doubao uploads during speech. Cloud recognition reuses the computer's saved keys and does not require Host ASR. The account still needs ASR access and quota; no provider credentials are stored on the device.

After negotiating `voice.feedback`, character expressions and status icons show normal conversation without overlaying stage labels. After eight seconds of continuous waiting, a DOWN cancellation hint appears for three seconds; the bottom controls stay in place. Short speech, empty recognition and turn failures retain a reason and play one local cue at the device volume. Recoverable failures preserve the connection and microphone choice. New valid input clears the reason; cancellation stays quiet. The audio worker suppresses capture around the cue.

Supported TTS models stream after final reply text is ready. An interrupted or overlong stream stops with feedback without replaying spoken audio. The complete conversation text remains in DSH. Older connectors remain compatible with reduced feedback.
