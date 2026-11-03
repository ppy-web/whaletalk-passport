---
name: whaletalk-connect-debug
description: 连接 WhaleTalk Passport（鲸语通行证）与 DSH，采集设备日志，排查配网、移动热点、认证、会话恢复和语音断续。适用于设备能启动后的连接与运行问题；不替代 ESP-IDF 安装或通用固件开发流程。
---

# WhaleTalk Passport 连接与排错

定位含 `main/online_runtime.c`、`tools/validate.py` 的应用根目录，读取当地 `AGENTS.md` 和 Git 状态。本技能随应用迁移，不假定插件、profile、工具链或串口在某个开发者的固定路径。

链路：设备 → DSH 插件连接器 → 宿主 ASR → 专用 Agent → 插件 TTS → 设备。独立固件仓库不含 DSH 侧连接器。先确认两边版本兼容，区分诊断与修复请求；诊断先收集证据，不自行修改、刷机或重置。

## 按故障阶段取证

- 首次接入、热点、多网卡、401、超时：读 [连接步骤](references/connection.md)。
- 会话恢复、卡顿、无声、重启：读 [运行排错](references/troubleshooting.md)。
- 需要更新固件：读 [构建与刷写](references/device-test.md)，将设备、镜像身份和数据影响绑定到本次授权。

从最早失败的一层开始：USB/启动 → Wi-Fi → DHCP → TCP → WebSocket 认证 → 协议/会话 → ASR → Agent → TTS → 播放回执。HTTP 前的超时不等于配对码错误，“正在监听”不等于设备已连接。

## 诊断工具

从应用根目录执行。`host`、`analyze` 仅用标准库；串口命令需 ESP-IDF Python 的 pyserial。地址和 COM3 均为示例，先枚举确认设备。

```powershell
python .agents/skills/whaletalk-connect-debug/scripts/diagnose.py ports
python .agents/skills/whaletalk-connect-debug/scripts/diagnose.py host --bridge-url http://192.168.137.1:3101 --prompt-code
python .agents/skills/whaletalk-connect-debug/scripts/diagnose.py serial --port COM3 --seconds 60 --output build/diagnostics/playback.log
python .agents/skills/whaletalk-connect-debug/scripts/diagnose.py analyze build/diagnostics/playback.log
```

串口采集不复位、不发指令、不截图；采音质时不要并行截图。常见凭据字段会脱敏，日志仍保留本地、分享前复核。配对码交互输入或经 `PASSPORT_PAIRING_CODE` 传入，不放入命令行、文档或 Git。

## 修复与验收

只修复已证实的问题，仅修改相关配置。不打印完整 profile，不删除会话身份绕过恢复失败，不关闭整机防火墙。管理员操作需准备明确范围并说明系统授权要求，已有授权覆盖的步骤直接继续。

分别报告连接、录音/转写、回复生成、真实听感、连续两轮、取消和重连。编译、`audio.done`、一次 `gaps=0` 均不能单独证明音质良好。区分证据与推断，说明未测项，完成后释放串口。
