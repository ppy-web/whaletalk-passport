# 鲸语角色与背景素材

设备内应用名为「鲸语」，仓库名保持 WhaleTalk Passport。默认大肥鱼。
长按 OK，进入「系统设置 → 界面」后短按 OK，按大肥鱼、鲸鱼娘、Claude、ChatGPT、仅背景的顺序切换。
长按 OK 返回首页。外观选择使用原有的 `dsh_voice/theme`，兼容已保存的旧选项，不重写连接配置。

## 文件与来源

- [大肥鱼图集](../assets/characters/big-fat-fish-atlas.png)于 2026-11-18 使用内置 imagegen 参照用户的干饭、得意和无语表情重绘。保留白色发箍、深蓝鲸鳍和长卷发，补全短裙、双脚和右侧鲸尾。参考图哈希和编辑提示词见[角色生成记录](../assets/characters/generation.json)。
- `assets/characters/whale-*.png` 是大肥鱼的十张 240×285 设备用预览。待机轮流显示端碗、夹饭入口和闭眼嚼饭。聆听举猫爪，思考托腮。回答时以得意姿势睁眼切换嘴型，播报约 1.2 秒后开始短眨眼，每 3.5 秒重复一次。故障或本轮失败时显示无语表情。
- 早期大肥鱼使用双耳机立绘，源文件供查阅。
  - `assets/characters/whale-girl-reference.png` 是用户提供的原插件 `assets/whale-girl-front.png` 角色参考。
  - `assets/characters/whale-girl-atlas.png` 与 `assets/characters/whale-girl-transparent-atlas.png` 保存相同的早期正面 3×2 表情图集。
  - `assets/characters/whale-girl-front-atlas-v2-alpha.png` 是 2026-11-05 使用内置 `imagegen` 编辑的双耳机立绘原图。[完整编辑记录与提示词](../assets/characters/generation.json)
  - `assets/characters/history/` 保存旧图集和中间稿。
- [鲸鱼娘图集](../assets/characters/whale-maid-atlas.png)及 [Claude 图集](../assets/characters/claude-atlas.png)：
  2026-11-14 用内置 imagegen 分别基于用户的 `鲸鱼娘2.jpg` 和 `Claude.jpg` 生成。
  每套保持同一角色构图，按待机、聆听、思考、说话、眨眼、错误排列。完整提示词及参考图哈希见
  [角色生成记录](../assets/characters/generation.json)。
- [ChatGPT 图集](../assets/characters/chatgpt-atlas.png)于 2026-11-18 用内置 imagegen 根据用户提供的银发龙娘立绘生成，保留银白长发、龙角、紫眼、结形发饰与白裙。六帧顺序与 Claude 相同，参考图哈希和提示词见[角色生成记录](../assets/characters/generation.json)。
- `assets/characters/*_avatar.c` 是各图集生成的 Flash 数据。大肥鱼十帧，鲸鱼娘、Claude 和 ChatGPT 各六帧。源 PNG 留在素材目录。
- `assets/backgrounds/*-source.png`：2026-11-05 内置 `imagegen` 生成的「晴海茶室」「星夜鲸梦」高清原图。
- `assets/backgrounds/seaside-tea.png`、`moonlit-dream.png`：240×320、256 色 PNG 参考图。
- `assets/backgrounds/whale_backgrounds.c`：晴海、星夜使用 240×320、256 色 BMP（共 155,756 字节）。
  四季预置图从示例 PNG 转成 120×160、256 色 BMP（共 81,112 字节）。这六张 BMP 由手机预览与设备共用。
  设备从每行的像素索引生成 RGB565，与自定义背景共用 480 字节行缓冲。
- [生成记录与提示词](../assets/backgrounds/generation.json)：两套背景及透明图集的生成工具、输入输出和完整提示词。
- `assets/backgrounds/examples/`：8 张可下载上传的 240×320 示例背景，四季图也是预置图的原始素材。
  见[预览、下载与上传说明](background-examples.md)及[独立生成记录](../assets/backgrounds/examples/generation.json)。

生成图并非 FoloToy SDK 自带的角色资产。项目保留参考图和提示词以便后续一致地制作素材；原 SDK、字体与参考代码来源仍见根目录 NOTICE。

## 重新编译素材

使用 `tools/host-requirements.txt` 中的 Pillow 环境运行：

```powershell
python tools/compile_whale_avatar.py
python tools/compile_backgrounds.py
python tools/validate.py --preview
python tools/render_preview.py
```

转换按图集分格、采样、裁剪和编码。大肥鱼的竖向帧采样为 216×288，置于屏幕坐标 (12, 38)，
下缘从屏幕第 295 行渐隐到第 319 行。两枚首页状态图标分列两侧，为双脚留出空间。
鲸鱼娘和 Claude 方形帧采样为 280×280，置于 (-20, 38)。
ChatGPT 方形帧采样为 248×248，置于 (-4, 49)，从屏幕第 248 行渐隐到第 296 行。
四套立绘在 35 像素状态栏下共用 240×285 的原生绘制区域。
每帧使用 256 项 RGB565/A8 调色板。I8 索引按行进行无损游程编码，密集行保留原索引；
行偏移表让 LVGL 可直接读取需要的行。四套立绘按切换顺序各用约 309、241、251、203 KiB Flash。
四套立绘共用专用 LVGL 解码器，每行输出 RGB565/A8，只使用 720 字节
像素缓存及对齐空间。不在设备上解压 PNG、缩放图片或分配完整图像缓冲，也不启用整屏离屏
透明图层。大肥鱼思考时维持托腮姿势，由顶部三点动效提示等待。鲸鱼娘、Claude 和 ChatGPT 思考时按 3.2 秒循环表情。
说话时有限频率切换嘴型。各帧底部的透明渐隐可直接叠在内置背景或用户自定义背景上。
自定义图由网页压缩为 WTBG v1：8 字节头、16 项 RGB565LE 调色板（32 字节）、
96×128 的 I4 像素（6,144 字节），高半字节在前，总长 6,184 字节。设备验证后存入 NVS
的 `bg_custom`，背景选择为 `background`；均保留在 `dsh_voice` 内。读取失败不擦除配置。
启动任务只保留 6,184 字节编码数据，不展开完整 RGB565 图。专用 LVGL 解码器直接
输出 240×320 的逐行像素，每行最多使用 480 字节静态缓存，不缓存整张解码图，
也不使用图片缩放路径。裁剪和放大索引在逐行解码中完成。内存不足时回退到内置图，不重置 NVS。
未上传自定义图时不分配其像素缓冲。实际音频并行余量仍需设备验收。

旧实现曾展开为 24,576 字节并交给 LVGL 缩放；设备上传自定义背景后出现剩余堆约 3 KB、
LVGL 临时图像分配失败并停在断言循环的故障，表现为停留「正在连接后端」且无法进入设置。
逐行实现针对这一内存峰值修正，不改变已保存图片格式，也不靠关闭看门狗或断言绕过问题。

## 原不透明图集提示词（2026-11-03，内置 imagegen）

Use case: identity-preserve. Create ONE production expression sprite ATLAS for a tiny 240x320 ESP32 voice companion screen, based on the reference character. Output a landscape image, ideally 1536x1024 pixels, with EXACTLY THREE COLUMNS and TWO ROWS of equal SQUARE cells, cells touching edge-to-edge, NO outer margins, NO grid lines. Each cell contains the SAME blue-haired chibi whale maid's front-facing BUST portrait down to her upper chest. Preserve reference identity: large sapphire blue eyes, rich blue flowing hair and curled forelock, white maid headband, navy-and-white maid outfit and bow, cyan whale-fin headphones. Simplify intricate clothing and highlights into clean polished anime cel shading and bold readable silhouettes for a 192x192 sprite. In EVERY cell keep identical camera, scale, framing, hair/headphone/torso position: full headband and curl inside the cell, generous 6% clear space around the silhouette, face prominent. Hands outside frame. Background in ALL cells is EXACTLY one flat opaque pale icy blue #E7F0FF, absolutely no gradients, shadows, textures or halos. Six expressions in exact reading order: top-left friendly idle, eyes open and small CLOSED-mouth smile; top-middle attentive listening, eyes open focused with slightly raised eyebrows and closed mouth; top-right thoughtful, eyes glance upward and small pursed closed mouth; bottom-left speaking, bright eyes and friendly OPEN mouth; bottom-middle blink, BOTH eyes gently CLOSED and closed-mouth smile; bottom-right mild concern/error, worried eyebrows and small closed-mouth frown, no tears. These are animation frames of one character, not six redesigned characters. NO text, labels, symbols, bubbles, UI controls, frames, watermarks or extra characters. Deliver only the single six-cell sprite atlas.

首页透明前景 `assets/ui/home-foreground.png` 由内置图像生成工具参考鲸语封面与首页风格生成；`tools/compile_home_foreground.py` 将上下边缘转换为与角色共用的调色板及逐行压缩格式，由同一解码器逐行输出 RGB565+A8。预览为 LVGL 合成状态，并非设备截图。

ChatGPT 龙娘专属前景 [chatgpt-foreground.png](../assets/ui/chatgpt-foreground.png) 由内置 imagegen 参考角色图集与现有前景构图生成，提示词见[生成记录](../assets/ui/chatgpt-generation.json)。切换到 ChatGPT 时自动使用，沿用同一压缩和解码流程。
