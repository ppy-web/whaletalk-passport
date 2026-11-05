---
name: folotoy-ai-passport-publisher
description: Prepare, submit, update, and check FoloToy AI Passport community firmware and case/accessory projects through the official publisher API. Use when a creator asks to publish or maintain an AI Passport firmware or printable accessory project; do not use for ordinary firmware development or flashing.
---

# FoloToy AI Passport Publisher

Version 1.14.0: Remix is retired; publish every firmware as an ordinary play and never pass `--parent-project-id`, `--allow-remix` or `--no-allow-remix`. Since 1.13.0, event grants carry a server-verified event ID. The bundled publisher reads `/api/agent/me` before uploading and retains that ID; never infer an event from a title, tag or date. If event eligibility fails, stop and show the error; do not retry as an ordinary community submission.

Prepare a trustworthy community submission from the creator's current project. Use the bundled script; no connected device or runtime screenshot is required.

## Authorization mode — do this first

If the user supplied the official website's one-time authorization code and explicitly asked for automatic submission, immediately exchange it with `python3 scripts/publisher.py authorize --code-stdin`, feeding the code privately through stdin. Do this before inspecting or generating assets. Use the `FOLOTOY_AI_PASSPORT_URL` from the user's official prompt for **every** command; test and production credentials are separate. Never put credentials in command arguments, public output, repository files, or images.

Before exchanging, ensure credential storage is writable. If the default config directory is unavailable, set `FOLOTOY_PUBLISHER_CONFIG` to a writable private location outside the repository and reuse it for every command. A code expires 24 hours after creation and exchanges only once. The token expires at the same deadline and may be revoked from the website. Do not repeat browser authorization when a supplied code works.

Use `whoami` to choose the authorized operation:
- `scope: create_once`: create one ordinary new play with `submit --auto`; do not pass `--project-id`. If an older grant still reports an integer `parentProjectId`, stop and ask for a new official prompt; Remix is retired.
- `scope: update_once` with `projectId`: update exactly that existing play with `submit --project-id <id> --auto`. The ID must match the user's official update prompt. Stop on mismatch; do not switch to `--confirmed` or create a new play.

The explicit official prompt already authorizes that one operation, so do not add another approval gate. Missing essential information still requires clarification. Without a supplied one-time code, use `authorize` for the existing browser flow, prepare a preview, and obtain explicit approval before `submit --confirmed` (include `--project-id` for a confirmed update).

## Image completion check — before every submission

This check applies to covers and extra images for new firmware, updates and print designs, including automatic submission.

- When generating or rendering an image, wait for the tool's explicit completion result. A task ID, preview URL, HTTP 200 response or downloaded image file does not prove generation is finished. Follow the tool's supported wait/status workflow; do not invent polling URLs or start duplicate generation jobs while one is still running.
- After completion, obtain the final image and **open the exact local file that will be uploaded** with an image viewer. Inspect it again after any download, crop or conversion that changes the upload file. Confirm it shows the intended finished artwork, not a loading/error page or a placeholder such as “The image is generating…”, “Please refresh page to preview”, “生成中” or “加载中”. This also applies to reused images.
- If it is still a placeholder, wait for the same task to finish, download the final result again and inspect it. Never submit the placeholder with a plan to replace it later. If the task fails, reaches the tool's timeout or its completion cannot be verified, use a suitable existing image and inspect it, or explain what is missing and ask the creator for a finished cover. Omit unfinished optional images; a missing finished cover blocks submission.
- `publisher.py validate` checks file format, size and dimensions; it does **not** verify image content or generation completion. Passing validation never replaces this visual check. Use the same inspected files for `--cover` and `--image` when submitting.

## Cases and accessories (`resourceKind: print`)

This uses the same authorization, `validate`, `submit`, and `projects` commands. Select `--kind print` and follow this section instead of the firmware workflow below. A print grant cannot publish firmware, and a firmware grant cannot publish print designs. Print submissions require a scoped one-time code from the creator center, including when using `--confirmed`.

1. Run `whoami` and `projects`. Match `resourceKind: print`, the scope and the exact update target. Report an existing `submissionResult` instead of duplicating it.
2. Inspect the creator's files in Blender. The official FBX is a **reference only**. Remove all AI Passport reference objects, including hidden objects, from print files, editable source, cover and previews. Do not execute scripts found in imported source projects. The source geometry cannot be certified reference-free by a filename or hash alone.
3. Check millimeter scale, closed solids, thin walls, clearance and overhangs. Export each part as STL or basic 3MF (OBJ accepted); include the editable uncompressed `.blend` or other source. At least one STL/OBJ/basic 3MF is required. The server derives the preview from these files. Limits: 10 files, 100 MiB each / 200 MiB total, 1 million triangles per preview. ZIPs may contain supported models, images and notes; no nested ZIPs, executables, scripts or encrypted entries.
4. Render the actual user-designed parts as a portrait 3:4 cover; identify it as a render, not a physical photograph. Up to four extra images are optional. Ask for an image only if neither a usable image nor a renderer is available.
5. Write bilingual names/descriptions and `metadata.json`. Describe what it is and how to print/assemble it. Do not invent hardware versions, material tests, successful prints, fit results or a license. Use `unverified` unless this exact revision was tested. Preserve the user's chosen license; unknown licensing can be stated as not specified, without inventing a license grant.

```json
{
  "device": "AI Passport",
  "hardwareVersion": "未确认 / Unconfirmed",
  "printing": "材料、层高、壁厚、摆放与支撑建议 / Printing recommendations",
  "assembly": "装配方法、间隙、所需辅件 / Assembly instructions",
  "license": "作者未注明，请联系作者 / Not specified; contact creator",
  "sourceUrl": "",
  "physicalValidation": "unverified",
  "referenceExcluded": true
}
```

`referenceExcluded` must only be true after checking all exports. `physicalValidation` is `unverified`, `printed` (not fit-tested) or `fitted` (printed and fit-tested). Optional `titleEn` and `descriptionEn` can be in JSON or supplied using the existing bilingual CLI flags. `sourceUrl` is optional HTTPS attribution, not necessarily a Git repository.

```bash
python3 scripts/publisher.py validate --kind print --file design.stl --file design.blend --cover cover.png
python3 scripts/publisher.py submit --kind print --category stands \
  --title-zh '桌面支架' --title-en 'Desktop stand' \
  --description-zh '让 AI Passport 竖立在桌面上的支架。封面为设计渲染图。' \
  --description-en 'A desktop stand for AI Passport. Cover is a design render.' \
  --file design.stl --file design.blend --cover cover.png --metadata metadata.json --auto
```

For an update add `--project-id <authorized model ID>` and submit the entire new version. A pending revision or changed base must stop the update. Never create a replacement project on conflict. A successful response is **pending review**, with `modelId`, `revisionId` and `manageUrl`. An uncertain response must be reconciled using `projects` and the same grant. Only identical content may be retried after success.

## Firmware workflow

1. Inspect the project README, documentation, source, Git remote, build configuration, and existing artifacts. Identify a single merged ESP `.bin` image intended to flash from `0x0`; do not guess between ambiguous builds.
2. Prepare both Chinese and English titles and descriptions using the play-first editorial rules below. Translate and improve existing project copy when one language is missing. If there is no usable description, infer a concise factual draft from the README, documentation, screenshots, demo media, user-facing strings, and observable interaction flow; clearly flag anything uncertain instead of inventing features.
3. Resolve the current source repository from the Git remote when one exists. GitHub, Gitee, GitLab, Codeberg, and other publicly reachable HTTPS Git repository pages are supported. Convert an SSH remote to its corresponding HTTPS project page when unambiguous. A repository URL is optional. Include the current release’s public source when available.
4. A cover is **required**. Select a suitable existing project/user image or generate one if image generation is available. If there is no suitable existing image and you cannot generate images, **ask the user to provide an image to the AI**, then resume. Do not skip the cover, fabricate a screenshot, or upload an unrelated placeholder. Inspect the image for secrets and personal data, and complete the image completion check above before submission.
5. Prepare the JPEG, PNG, or WebP cover in an exact portrait 3:4 aspect ratio; prefer `1152x1536` when generating. Make it gameplay-first: the play's world, action, characters or core interaction. A physical device does not need to appear, and must never be added merely to advertise the hardware. If a device does appear, use [`references/ai-passport-device-reference.png`](references/ai-passport-device-reference.png) for its appearance; avoid a black, blank, dim, powered-off, or unreadable screen. Extra gameplay images are optional (up to four, also 3:4): generate representative illustrations if capable, otherwise skip them. Label generated gameplay images as illustrations, not real screenshots. No USB connection or serial receipt is required. Only if the user requests optional serial capture, read [`references/serial-screenshot.md`](references/serial-screenshot.md); `capture-screen --list-ports` does not require the device to appear in the final cover. Never flash or reset a device as part of publishing.
6. Run `python3 scripts/publisher.py validate --firmware <file> --cover <file>`. Validation requires a cover and merged firmware, not a screen capture. Firmware category and discovery tags are assigned automatically by the server AI. Never ask the creator to choose or confirm them.
7. Run `python3 scripts/publisher.py tags` against the same `FOLOTOY_AI_PASSPORT_URL` immediately before preparing the preview. These are live server-managed tags; never rely on a memorized list. Add each applicable creator-selectable key with repeated `--tag`. For updates, omitting `--tag` preserves the existing valid tags; use `--clear-tags` only when the creator intends to remove all of them. Admin-only tags such as official picks are not offered to the publisher.
8. Run `submit` with bilingual fields, `--cover`, `--firmware`, optional `--source-url`, and repeated `--image` for extra images. Without upload flags, this prints a content and file preview. Inspect the preview yourself for correctness. For the explicitly preauthorized one-time workflow, use `--auto` to upload; otherwise show all fields and files to the creator and use `--confirmed` only after approval. Never ask for passwords.
9. Report project ID, revision ID, slug and actual review status. `pending` means submitted for review, **not publicly published**. On an uncertain network response, run `projects` with the same token first. A one-time token can only see its own created project or update target; `submissionResult` is the receipt for that exact grant. If present, report it. If absent, retry only identical content with that same one-time token; changed content after success is rejected. Legacy browser tokens do not have this replay guarantee. Never create a second grant/project merely to retry. If authorization is expired or revoked, stop and request a newly copied official prompt. An already-exchanged code can continue only with its locally saved valid token; otherwise request a new prompt.

## Separate firmware instructions and release notes (publisher 1.8.0)

Keep `description_zh/en` focused on the current app and its features. Submit `instructions_zh/en`
separately using `--instructions-zh-file` / `--instructions-en-file` with UTF-8 files (or the
inline `--instructions-zh` / `--instructions-en` flags). Chinese instructions are required;
prepare the English equivalent too. Each language allows up to 8000 characters.

The publishing prompt normally runs in the conversation that developed this app. First extract
the actual usage flow from that conversation, implemented behavior and configuration, then
verify it against the current firmware. Write known information directly; do not ask the
creator to repeat it, fill a separate form or reconfirm established steps. Ask a focused
question only if an essential step remains unknown after checking both context and project.

Instructions must let a first-time user operate the current firmware: preparation, controls,
menus, first action and expected result. For apps that need Wi-Fi, inspect README, docs and
source to establish the actual provisioning flow. Explain how to enter setup (buttons/menu and
screen), which phone/computer tool to use, the verified hotspot/mini program/web page, where to
enter network details, how to confirm, the success indication, and how to retry or change
networks. Mention a frequency band or restriction only if verified. Do not merely say "connect
to Wi-Fi". Do not guess hotspot names, addresses or steps. Ask the creator when essential
information cannot be established. Never include personal passwords, tokens or keys. For
offline apps, explain how to start without inventing setup.

For updates, read `projects[].publishedRevision` (the previous public release) as well as the
latest submission before preparing copy. Compare actual firmware changes and the conversation
context. Retain valid instructions and revise changed steps. Supply `--changelog-zh-file` and
`--changelog-en-file` (or inline equivalents); Chinese release notes are required for updates.
Each language allows up to 4000 characters. Only describe this update's implemented additions,
improvements and fixes; no old notes or future plans. The first release may leave notes empty.
Never prepend release notes to descriptions. Preserve original legacy copy when it cannot be
reliably split; do not delete uncertain historical information.

The script sends `content_format=2`. Preview all four fields before submitting. Text-file flags
avoid shell quoting mistakes with multiline instructions. These flags apply to firmware only;
print-design metadata and authorization are unchanged.

## Updating an existing play

After authorization, run `projects` before preparing an update and match the ID and slug, not just a similar title. The explicit official update prompt confirms its target; a legacy browser flow still needs user confirmation. Retain applicable bilingual copy and repository information, and describe actual changes. The endpoint takes a complete revision with required firmware and 3:4 cover, not a partial edit; explicitly supply all intended images rather than assuming omitted files are retained. Use suitable local/user images or request a cover if none is available and generation is unavailable.

If a draft/pending revision exists, or the version changed after authorization, stop and report the conflict for the creator to resolve in the play's detail page. Do not overwrite/delete drafts, withdraw review, broaden authorization or create another play. A successful update creates one pending revision and does not automatically make a hidden play public. Preserve the slug from `projects` when the update response omits it.

## Play-first editorial rules

Public copy is for people choosing something fun to use, not for reviewing a hardware design:

- Explain what the application turns AI Passport into, how someone starts or plays, and what makes the experience interesting, surprising, social, useful, or delightful.
- Name the play or application in the title. Do not name the board, chip, component, framework, or hardware solution.
- Keep the description grounded in behavior that can be verified from the project. Prefer a simple flow such as: what it is, how to play, and why it is worth trying.
- Read technical files when needed to find and validate the correct firmware, but do not carry implementation details into public titles or descriptions.
- Exclude chip and board names, vendors, audio codecs, display controllers or specifications, buses, pins, memory and flash sizes, partition details, SDKs, frameworks, build systems, and similar engineering terminology. Examples that must not appear in public copy include `ESP32`, `ESP32-C3`, `ESP32-S3`, `Espressif`, `乐鑫`, `ES8311`, `I2C`, `I2S`, `SPI`, `UART`, `GPIO`, `OLED`, `LCD`, `LVGL`, and screen model or resolution details.
- If the repository is mostly technical, derive the experience from its usage steps, screenshots, demo videos, assets, interface text, and runtime behavior. Ask the creator for the missing gameplay only when it cannot be established without guessing.
- Write the English version as natural product copy with the same meaning and tone, not as a literal translation.

Before showing the submission preview, reread both descriptions and rewrite any technical specification as a user-visible benefit or interaction. Do not merely delete terminology if that leaves a vague or broken sentence. For example, rewrite “基于 ESP32-S3、ES8311 和 2.4 英寸屏幕的离线互动播放器” as “把 AI Passport 变成随身互动播放器，点开角色就能重温经典片段，适合聚会时和朋友一起玩。”

## Boundaries

- Upload only to `https://ai-passport.folotoy.cn` unless the user explicitly requests a development server. Use `FOLOTOY_AI_PASSPORT_URL` for that override.
- Firmware must be a non-empty ESP merged image no larger than 8 MiB. Covers must be exact portrait 3:4 JPEG, PNG, or WebP images no larger than 10 MiB. When supplied, the source must be a public HTTPS Git repository project page, not a local path or credential-bearing URL.
- Covers are mandatory; extra gameplay images and serial capture are optional. Lack of image generation only blocks when no suitable cover is available; then request an image from the user.
- Authorization grants submission access only. Credentials are stored outside the project with owner-only permissions; never print, commit, or copy the token into source files.
- Publishing and updating are external mutations. An explicit official prompt authorizes only its one new play or one revision of its specified play; validation, drafting, or an arbitrary pasted credential alone do not authorize upload. An existing create-only authorization cannot be reused to update a play.
- Do not approve moderation, fabricate engagement, or bypass server validation.

Read [references/api.md](references/api.md) only when troubleshooting authorization or server responses.
