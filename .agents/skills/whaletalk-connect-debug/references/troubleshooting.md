# 根据运行日志缩小范围

| 证据 | 最小下一步 |
| --- | --- |
| `wifi disconnected reason=201` | 当前 SDK 表示找不到 AP，核对 SSID、2.4 GHz 覆盖、热点是否开启 |
| TCP 超时、HTTP 0、`ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT` | 查网段、监听、入站规则；lwIP errno 119 不按 Windows errno 表解释 |
| HTTP 401/403 | TCP 已通，核对当前码、私有网段、是否带浏览器 Origin |
| HTTP 404 | 核对端口与 `/api/realtime`，不要连网页端口 |
| HTTP 503、ownership busy | 检查服务、连接数和设备占用，不接管其他设备 |
| `passport-asr-unavailable` | 检查宿主 ASR 选择和模型准备，不自动换供应商 |
| “请检查 DSH 对话配置” | 可能是预设或专用会话恢复失败，继续看宿主，不能据此认定唯一原因 |
| `passport-preset-changed` / “请在 DSH 刷新预设” / 旧版“请恢复原来的 DSH 预设” | 旧会话绑定另一预设；新版连接器选择可用预设并点击“刷新预设并新建会话”。只改 ID 或重连不会切换旧会话。ID 可以是中文，应以运行中宿主的预设列表为准，不根据名称猜测 |
| `passport-recognition-failed` | 分查 PCM/WAV、ASR、取消，保留原错误边界 |
| `passport-speech-failed` | 先独立试听宿主音频，再查 WAV → PCM → 设备 |
| `playback queue empty` | 区分供音不足、网络抖动和设备调度 |
| `slow audio write` | 看 I2S、CPU/界面占用和格式切换，不直接认定硬件坏 |
| panic、watchdog、stack canary | 获取精确镜像与匹配 ELF 再解码，不拿后来重编译的 ELF 解释旧崩溃 |

## 会话与热更新

身份记录在 DSH 用户目录 `.dsh-xiaomi-tts/passport/`，按 profile 和组件位置隔离，不在固件仓库。只读定位到的身份，不扫描其他聊天正文、不删除记录、不失败后另建替代会话。

开发构建会清理重写插件 `lib/`。测试和打包不要并行写它；依次完成后再通过宿主支持的机制加载最终版本。外链插件未必在 HMR watch roots，源文件更新不等于实例已更新。

更新后会话恢复失败时，查预设注册、持久化、写入占用和宿主错误。授权范围内可重启**已确认的当前实例**，沿用相同 profile、环境、启动参数和身份；不终止其他 Node、不清空配置。恢复后核对 `connected` 和原会话身份再试听。

## 播放断续：这次验证过的经验

PCM16LE 单声道，输入 16 kHz，输出 24 kHz。播放每秒 48,000 字节，960 字节为 20ms。音频先完整合成再发送，可先对比电脑 WAV 与设备声音。

一次 Windows / Node 24.18 实测：循环 `await delay(20)` 平均约 31.2ms，3 秒音频耗时约 4.685 秒，供音约为需求的 64%。这是当次测量，不是所有机器的固定定时精度。

优化在 **DSH 插件** `src/passport/playback.ts`：按单调时钟和音频时长调度，约 160ms 有界提前量；长停顿后重校准，避免无限集中补发；取消立即停止，保留音频顺序与末包；`audio.done` 后仍等匹配的 `playback.ended`。同机实际函数发送 3 秒音频约 2.85 秒（含提前量），用户确认实机流畅。独立设备仓库不包含此 Host 修复，需要兼容的插件版本。

设备无 PSRAM，播放队列 32 项，一个 wire packet 可占多项，不把项数直接当时长，不无限加缓冲。I2S DMA 为 6×240 帧，24 kHz 理论容量约 60ms，实际余量取决于占用。

- `capture_drops` 是录音**上行**计数，不是扬声器丢包。
- 当前 `playback stats gaps` 只记录达到 60ms 的队列断供，`gaps=0` 可能漏掉小间隙，不能代替听感。
- 同看 `max_write_ms`、缓冲余量、Wi-Fi 重连与最大连续内存块。音质采样时不截图、不存设置、不写 Flash，避免干扰。
- 轮次边界可切换 16/24 kHz，一段话中反复重开 codec 则需另查。

用固定短句和 15–20 秒回复复现，比较 WAV、日志与听感，再测连续两轮和 DOWN 取消。仍异常时根据实际 feed gap 查任务、GUI、DMA、热点抖动，不先换硬件或提高所有优先级。
