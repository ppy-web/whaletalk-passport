# 鲸语角色与背景素材

设备内应用名为「鲸语」，仓库名保持 WhaleTalk Passport。默认鲸鱼娘。
长按 OK，进入「系统设置 → 界面」后短按 OK，按鲸鱼娘、薄荷表情、鲸鱼娘2、Claude 的顺序切换。
长按 OK 返回首页。外观选择使用原有的 `dsh_voice/theme`，兼容已保存的旧选项，不重写连接配置。

## 文件与来源

- `assets/characters/whale-girl-reference.png`：用户提供的原插件 `assets/whale-girl-front.png` 角色参考，保留蓝发、鲸鳍耳机、头饰和女仆装特征。
- `assets/characters/whale-girl-atlas.png`：当前正面 3×2 表情图集，顺序为待机、聆听、思考、说话、眨眼、错误；两侧都有耳机、青色蝴蝶结和鲸鳍装饰，身体延伸至腰部及围裙，底部以透明度渐隐。
- `assets/characters/whale-*.png`：定尺为 240×285 的设备用预览，已包含放大、居中及顶部裁剪。
- `assets/characters/whale-girl-transparent-atlas.png`：与当前图集相同的 RGBA 数据，也是编译脚本使用的输入。
- `assets/characters/whale-girl-front-atlas-v2-alpha.png`：2026-11-05 使用内置 `imagegen` 编辑的最终双侧耳机、腰部延展与渐隐原图；[完整编辑记录与提示词](../assets/characters/generation.json)。
- `assets/characters/history/`：旧版正面图集及未采用的中间稿，仅作素材来源记录，不参与构建；原始侧身参考图保留在原路径。
- [鲸鱼娘2 图集](../assets/characters/whale-maid-atlas.png)及 [Claude 图集](../assets/characters/claude-atlas.png)：
  2026-11-14 用内置 imagegen 分别基于用户的 `鲸鱼娘2.jpg` 和 `Claude.jpg` 生成。
  每套保持同一角色构图，按待机、聆听、思考、说话、眨眼、错误排列。完整提示词及参考图哈希见
  [角色生成记录](../assets/characters/generation.json)。
- `assets/characters/*_avatar.c`：三套图集生成的 Flash 数据，每套六帧；源 PNG 不打包进固件。
- `assets/backgrounds/*-source.png`：2026-11-05 内置 `imagegen` 生成的「晴海茶室」「星夜鲸梦」高清原图。
- `assets/backgrounds/seaside-tea.png`、`moonlit-dream.png`：240×320 PNG，供配置网页预览。
- `assets/backgrounds/whale_backgrounds.c`：晴海、星夜使用 240×320 RGB565（共 307,200 字节）。
  四季预置图从示例 PNG 转成 120×160、256 色 BMP（共 81,112 字节），手机预览与设备共用这份 Flash 数据。
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

转换仅做规则分格、定尺、裁剪和像素编码，角色绘制由内置生成工具完成。每个高清方形帧
先采样为 280×280，置于屏幕坐标 (-20, 38)，在 35 像素状态栏下输出
240×285 的原生绘制区域。头顶呆毛完整露出，脸部位于屏幕中部，腰部渐隐延伸到下方按钮后面；状态图标移至顶部。
每帧使用 256 项 RGB565/A8 调色板。I8 索引按行进行无损游程编码，密集行保留原索引；
行偏移表让 LVGL 可直接读取需要的行。三套立绘各用约 244、241、251 KiB Flash。
运行时三套立绘共用专用 LVGL 解码器，每行输出 RGB565/A8，只使用 720 字节
像素缓存及对齐空间。不在设备上解压 PNG、缩放图片或分配完整图像缓冲，也不启用整屏离屏
透明图层。思考时按 3.2 秒循环有限帧，说话时有限频率切换嘴型。
每套六帧保持相同人物与构图。底部的透明渐隐编码在各帧中，可直接叠在六套内置背景或用户自定义背景上。
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
