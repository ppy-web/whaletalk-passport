English · [简体中文](guide.zh_CN.md)

# WhaleTalk Passport

AI Passport becomes a voice terminal for the local DSH instance:
microphone → Wi-Fi → plugin Passport adapter → Host ASR → dedicated Agent
conversation → plugin TTS → device speaker. No Qwen Audio Agent backend is
required. The computer must keep DSH running.

## Hardware and interface

ESP32-C3, 8 MB Flash, no PSRAM, 240×320 screen, ES8311 audio and three buttons.
The app uses the official BSP and adapts Qwen Voice Bean's expressive face and
conversation flow. It boots directly into a mint voice companion, with setup,
connection, ready, listening, thinking, speaking, error, settings and confirmed
reconfiguration screens. Main labels and states use 16px fonts and the provisioning address uses 20px. Chinese labels use compiled font subsets; arbitrary
model-response transcripts are not displayed.

| Button | Home action |
| --- | --- |
| Short OK | Toggle microphone; initially closed |
| Short UP | Cycle volume, including mute |
| Short DOWN | Cancel recognition, generation or playback |
| Hold OK | Open settings; hold again to return home |

In settings, UP/DOWN select and OK confirms. Reconfiguration needs confirmation;
cancelling keeps existing settings. Audio uploads only after the user enables
the microphone, pauses during recognition/thinking/playback, and resumes after
speaker completion is acknowledged. No automatic voice interruption or acoustic
echo cancellation is provided. Handle tool approvals in DSH.

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

1. Prepare and separately test Host ASR (such as SenseVoice), the default conversation model and plugin TTS.
The default Agent preset is the plugin’s `whale-girl` (enable the Whale Girl preset component first). The default workspace is `~/.dsh-xiaomi-tts/passport/workspace`, created before a new conversation. Existing conversations keep their preset and directory.

2. Enable AI Passport Connector and **generate a pairing code and enable the connection**. It displays available computer IPs and **four digits**, and listens on `0.0.0.0:3101`. Enter the computer LAN IP on the device, never `0.0.0.0`, `127.0.0.1` or the device address.
3. Join the displayed `DSH-Voice-*` hotspot on a phone; **no password is needed**. Open `http://192.168.4.1` and enter the computer IP plus the same four digits, including leading zeros.
4. First-time setup also needs 2.4 GHz Wi-Fi credentials. Keep saved Wi-Fi to change only the computer address or pairing code. Port 3101 is under Advanced settings.
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
