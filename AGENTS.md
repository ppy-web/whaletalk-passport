# WhaleTalk Passport development

This is a standalone ESP32-C3 AI Passport firmware application. The DSH Host
connector remains in dsh-xiaomi-tts; do not duplicate its ASR, Agent or TTS
implementation here. Keep the GCP 7.0.0 audio subset and saved device identity,
NVS namespace, endpoint and partition compatibility unless a migration is requested.

- Read `.agents/skills/whaletalk-connect-debug/SKILL.md` for connection/debugging.
- Run `python tools/validate.py --static` for state and font checks, then
  `python tools/validate.py` in ESP-IDF 5.5.3 for a full firmware gate.
- Run `python tools/validate.py --preview` after UI changes and inspect the
  actual LVGL renders. They are synthetic, not device screenshots.
- Run `python -m unittest discover -s .agents/skills/whaletalk-connect-debug/scripts -p 'test_*.py'`
  after editing the diagnostic helper.
- Main behavior is in `main/`; reusable board drivers stay in `components/bsp/`.
  Use BSP pins and APIs, the LVGL lock outside its task, and short button callbacks.
- This board has no PSRAM. Budget internal RAM, task stacks, queues and DMA.
  Verify Chinese glyph coverage after changing labels.
- `sdkconfig.defaults` and `dependencies.lock` are maintained inputs;
  `build/`, `managed_components/` and `.tools/` are generated or local.
- Preserve the current partition layout, including cardid and Recovery.
  Verify archive hashes before delivery; do not flash from a build command.
- Device writes require authorization for the device, exact image and data
  impact. Existing explicit authorization need not be requested twice.
- Test actual behavior, protocol, cancellation, cleanup and error boundaries.
  Do not test source spelling, CSS/dimensions or exact documentation prose.
- Keep guides, protocol notes, source attribution and local links current.
  Do not commit logs, credentials, NVS, firmware/ELF or local toolchain paths.
