[English](guide.md) · 简体中文

# 鲸语 · WhaleTalk Passport

让 AI Passport 成为本机 DSH 的语音终端。链路为：设备麦克风 → Wi-Fi →
插件 AI Passport 组件 → 宿主 ASR → 专用 Agent 会话 → 插件 TTS → 设备扬声器。
不需要 Qwen Audio Agent 后台。电脑必须持续运行 DSH。

## 硬件与界面

ESP32-C3、8 MB Flash、无 PSRAM、240×320 屏幕、ES8311 音频、三个实体按键。
使用官方 BSP，参考 Qwen Voice Bean 的表情与对话方式，设备内显示名称为「鲸语」。
首次启动默认鲸鱼娘界面，也可切换原薄荷表情；两套界面共用配网、连接、待机、
聆听、思考、说话、错误、设置和重新配置确认。
中文使用随应用构建的字形子集；主要说明与状态为 16px，配网地址为 20px。不显示任意模型回复全文。

| 按键 | 首页操作 |
| --- | --- |
| OK 短按 | 开关麦克风；默认关闭 |
| UP 短按 | 按 0、25、50、75、90、100% 循环调整音量，0 为静音 |
| DOWN 短按 | 取消识别、生成或播报 |
| OK 长按 | 打开设置；再次长按返回首页 |

设置中 UP/DOWN 选择、OK 确认。重新配网需要二次确认，取消保留当前配置。
长按 OK 进入设置，选中第一项「界面」，短按 OK 在「鲸鱼娘」与「薄荷表情」间切换；
再长按 OK 返回首页。选择会保存并在重启后恢复，不修改 Wi-Fi、配对码或会话。
播报/思考期间延后保存，失败时提示「界面未保存，请重试」。
鲸鱼娘拥有待机、聆听、思考、说话、眨眼与错误六种表情，素材和生成方式见
[角色素材说明](whale-girl-assets.md)。
设置第二项「息屏」短按 OK 按「1 分钟 → 3 分钟 → 10 分钟 → 不息屏」循环切换，
立即生效并保存，重启后沿用；未设置时默认 1 分钟。保存失败时提示「息屏未保存，请重试」，
再次切换可重试保存。思考和播报期间延后保存。
达到所选无操作时间后自动关闭背光并停止表情刷新，网络连接和已开启的麦克风继续工作。
选择「不息屏」时始终保持亮屏。
息屏后的第一次按键仅唤醒屏幕，再次按键执行原操作；进入思考或播报时自动亮屏，
思考和播报结束后重新计时。配网期间或按键不可用时保持亮屏，便于连接和排错。
该功能关闭屏幕背光，不进入设备深睡；持续联网与收音仍会耗电，续航改善需实机测量。
录音仅在用户开麦后上传；识别、思考和播放期间暂停上传，扬声器播放完成并
回执后恢复。不含声学回声消除或声音自动打断。工具审批请在 DSH 中处理。

## 开发与构建

需要 Python 3.10+ 和 ESP-IDF **5.5.3**；重新生成字体时额外需要 Node.js/npx。
在本目录的 PowerShell 中运行：

```powershell
./tools/setup.ps1
. ./tools/activate.ps1
python tools/validate.py --firmware
```

脚本默认把 ESP-IDF 与工具安装到本仓库 `.tools/esp-idf-v5.5.3` 和
`.tools/espressif`，不修改全局 PATH 或 DSH profile。可传 `-IdfPath`、
`-ToolsPath`、`-HostToolsPath` 复用其他目录，后续激活使用相同参数。
中国大陆下载可仅对当前终端设置 `IDF_GITHUB_ASSETS=dl.espressif.cn/github_assets`、
`IDF_COMPONENT_STORAGE_URL=https://components-file.espressif.cn`。

安装脚本还会在仓库 `.tools/host` 中安装独立的 Zig C 编译器和 Pillow，
激活脚本只向当前终端 PATH 加入编译器。可用该环境的 Python 运行
`tools/render_preview.py`，将 LVGL 预览转换为 PNG 和总览图。

首次构建按 `dependencies.lock` 下载组件。生成文件位于忽略的 `build/`、
`managed_components/`，不应手改。固件 gate 使用独立的
`build/firmware-build/sdkconfig`，以受版本管理的 `sdkconfig.defaults` 为准。

```powershell
python tools/validate.py --static
python tools/validate.py --preview
python tools/validate.py
```

静态检查需要本机 C 编译器（`CC` 指向可执行文件，或 PATH 中有 `cc`、`gcc`、
`zig`）；预览额外需要 CMake、Ninja 和构建已下载的 LVGL。预览使用应用的真实
LVGL 渲染器、模拟状态，并检查可见文字、字形、边界与按键。图片写入
`build/preview/*.ppm`，不能作为设备截图或硬件验收。

更改界面中文后执行 `python tools/generate_online_font.py`，需要 Node.js/npx。
生成器默认使用下载的 LVGL 中 Source Han Sans 字体，也可设置
`ONLINE_FONT_SOURCE` 指向有许可的完整字体；随后运行 `--check` 检查覆盖。

连接器和发送节拍补偿属于独立维护的 DSH 插件；其 pnpm/WebSocket 测试需在插件仓库执行。
本仓库的 C 状态测试、固件 gate、LVGL 预览与诊断工具可独立运行。

## 连接本机 DSH

1. 在 DSH 中准备 ASR（例如 SenseVoice）、默认对话模型和本插件 TTS，分别测试能转写、能对话、能发声。
默认 Agent 预设为本插件的 `whale-girl`（需启用鲸鱼娘预设组件），默认工作目录为 `~/.dsh-xiaomi-tts/passport/workspace`，创建新会话前自动创建目录。已有会话保留原预设和目录。

2. 打开 AI Passport 连接器，点击 **生成配对码并开启连接**。页面显示可用的电脑 IP 和 **4 位数字配对码**，自动使用 `0.0.0.0:3101` 监听。设备填写电脑的局域网 IP，不能填 `0.0.0.0`、`127.0.0.1` 或设备自己的地址。
3. 手机连接设备显示的 `DSH-Voice-*` 热点，**无需密码**。浏览器打开 `http://192.168.4.1`，填写电脑 IP 和相同的 4 位配对码，包含前导 0。
4. 首次使用选择 2.4 GHz Wi-Fi 并填密码；已有网络时勾选 **保留已连接的 Wi-Fi**，只修改电脑地址或配对码。端口在高级设置中，默认 3101。
5. 保存后设备重启，显示“准备好了”时短按 OK 开麦，说完停顿，听完回复再继续。长按 OK 进入设置可重新配网。

配对码不区分开发和正常模式，都使用 4 位数字，并仅接受本机与局域网设备。既有长令牌配置仍能连接，便于旧固件过渡。电脑页面显示监听、ASR 与设备连接状态；设备区分配对码不一致、电脑未开启连接、端口错误和超时。旧固件不支持短码，需要更新应用。

设备连电脑 Windows 移动热点时，电脑地址通常为 `192.168.137.1`，以实际热点网卡为准；
连接器会同时列出热点和普通局域网地址。网页 `3080` 与设备 `3101` 独立，
网页只绑定本机不影响设备语音。热点超时与限定范围的防火墙命令见
[连接排错技能](../.agents/skills/whaletalk-connect-debug/references/connection.md)。

监听地址与端口要能通过本机防火墙。多网卡电脑选择设备当前 Wi-Fi 能访问的电脑 IP；电脑若正在连接设备配网热点，不要把这个临时网络误当作日常 Wi-Fi。设备热点只用于配置，电脑需持续运行 DSH。

Wi-Fi 密码与配对码仅保存在设备 `dsh_voice` NVS 命名空间（未加密）；配置页面不回传已保存的密码或配对码，设备不保存录音。会话恢复与工具权限由 DSH 管理，工具审批在宿主完成。详见根目录 [插件用户手册](https://github.com/ppy-web/dsh-plugin-xiaomi-mimo-tts/blob/main/handbook.md)。

## 固件文件与实机测试

构建通过后，`build/WhaleTalk-Passport-full.bin` 是从 **0x0** 写入的合并镜像；
`build/firmware/<SHA-256>/` 保留同一次构建的 ELF、MAP 和 manifest。

```powershell
python tools/archive_firmware.py verify build/firmware/<SHA-256>
```

沿用参考项目的 3 MB 应用分区、`cardid@0x356000`、`recovery@0x700000`
及开机按住 UP 五秒的 Recovery hook。合并镜像不附带新的 Recovery 镜像；
该恢复入口仅在设备原有 Recovery 存在且兼容时可用。
合并刷写可能重置 NVS 配网信息；应用单独的 `.bin` 不能写到 0x0。
保留设置时需要先确认当前设备分区与目标布局兼容，再使用分段刷写。

构建与 USB 枚举不会自动刷机。确定设备、固件哈希与数据影响并获得本次刷写授权
后才写入。实机验收需分别检查中文界面、三键操作、真实录音识别、TTS 音质、
两轮连续对话、DOWN 取消、静音和 Wi-Fi 断开重连。成功编译或模拟测试不代表
这些硬件检查通过。

来源与许可见 [NOTICE](../NOTICE)，开发方式参考
[官方教程](https://ai-passport.folotoy.cn/guides/create-a-play-with-agent/) 与
[Qwen Voice Bean](https://github.com/liutaocode/esp32demo/tree/main/examples/qwen-voice-bean)。
