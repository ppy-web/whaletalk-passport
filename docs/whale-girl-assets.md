# 鲸语双界面素材

设备内应用名为「鲸语」，仓库名保持 WhaleTalk Passport。默认鲸鱼娘，另一套为原薄荷表情。
设置第一项按 OK 切换，长按 OK 返回；外观选择单独保存，不重写连接配置。

## 文件与来源

- `assets/characters/whale-girl-reference.png`：用户提供的原插件 `assets/whale-girl-front.png` 角色参考，保留蓝发、鲸鳍耳机、头饰和女仆装特征。
- `assets/characters/whale-girl-atlas.png`：内置 `imagegen` 于 2026-11-03 生成的 3×2 表情图集，顺序为待机、聆听、思考、说话、眨眼、错误。
- `assets/characters/whale-*.png`：定尺为 192×192 的设备用预览。
- `assets/characters/whale_avatar.c`：由构建工具转换的六帧不透明 RGB565 数据，合计 442,368 字节，存放于 Flash。

生成图并非 FoloToy SDK 自带的角色资产。项目保留参考图和提示词以便后续一致地制作素材；原 SDK、字体与参考代码来源仍见根目录 NOTICE。

## 重新编译素材

使用 `tools/host-requirements.txt` 中的 Pillow 环境运行：

```powershell
python tools/compile_whale_avatar.py
python tools/validate.py --preview
python tools/render_preview.py
```

转换仅做规则分格、定尺和像素编码，角色绘制由内置生成工具完成。运行时直接读取 Flash
中的 RGB565，不解压 PNG，不分配完整图像缓冲，不做缩放/透明蒙版。静态表情仅在状态改变
或眨眼时刷新，说话时以有限频率切换嘴型；相同背景与文本不反复重绘。

## 最终生成提示词（内置 imagegen）

Use case: identity-preserve. Create ONE production expression sprite ATLAS for a tiny 240x320 ESP32 voice companion screen, based on the reference character. Output a landscape image, ideally 1536x1024 pixels, with EXACTLY THREE COLUMNS and TWO ROWS of equal SQUARE cells, cells touching edge-to-edge, NO outer margins, NO grid lines. Each cell contains the SAME blue-haired chibi whale maid's front-facing BUST portrait down to her upper chest. Preserve reference identity: large sapphire blue eyes, rich blue flowing hair and curled forelock, white maid headband, navy-and-white maid outfit and bow, cyan whale-fin headphones. Simplify intricate clothing and highlights into clean polished anime cel shading and bold readable silhouettes for a 192x192 sprite. In EVERY cell keep identical camera, scale, framing, hair/headphone/torso position: full headband and curl inside the cell, generous 6% clear space around the silhouette, face prominent. Hands outside frame. Background in ALL cells is EXACTLY one flat opaque pale icy blue #E7F0FF, absolutely no gradients, shadows, textures or halos. Six expressions in exact reading order: top-left friendly idle, eyes open and small CLOSED-mouth smile; top-middle attentive listening, eyes open focused with slightly raised eyebrows and closed mouth; top-right thoughtful, eyes glance upward and small pursed closed mouth; bottom-left speaking, bright eyes and friendly OPEN mouth; bottom-middle blink, BOTH eyes gently CLOSED and closed-mouth smile; bottom-right mild concern/error, worried eyebrows and small closed-mouth frown, no tears. These are animation frames of one character, not six redesigned characters. NO text, labels, symbols, bubbles, UI controls, frames, watermarks or extra characters. Deliver only the single six-cell sprite atlas.
