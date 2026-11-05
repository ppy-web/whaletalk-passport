# 连接与 Windows 移动热点

## 最少配置

1. 同一个 DSH 实例准备并独立测试 ASR、默认模型、TTS，启用连接器和鲸鱼娘预设。连接器新会话默认 `whale-girl`，目录 `~/.dsh-xiaomi-tts/passport/workspace` 自动创建；已有会话沿用原预设和目录。
2. 在连接器生成 4 位码。监听 `0.0.0.0:3101` 或对应电脑网卡；`127.0.0.1` 仅本机。重新生成会使旧码失效，不使用历史示例码。
3. 设备长按 OK → 设置 → 重新配置 → 确认，连接开放热点 `DSH-Voice-*`，打开 `http://192.168.4.1`。首次选择 2.4 GHz Wi-Fi；已保存网络可保留，仅改电脑 IP 和配对码，端口在高级设置。
4. 保存重启后确认连接，短按 OK 开麦。UP 调音量，DOWN 取消，长按 OK 进入设置。默认麦克风关闭。

可选择电脑所在局域网的 Wi-Fi 或电脑的 2.4 GHz 热点，进入配网前先开启。找不到网络时点“刷新 Wi-Fi 列表”重新扫描，隐藏网络可手动输入；无需重新刷写固件。扫描期间配网页可能短暂变慢，等待结果即可。

开放热点和 4 位码是当前产品选择，不擅自恢复复杂密码。保存的 Wi-Fi 密码和配对码不能回传网页；短码仍需匹配，且只接受本机与私有局域网设备。

## 分清三个地址

| 地址 | 用途 |
| --- | --- |
| `192.168.4.1` | 设备配网页面，不作为电脑地址 |
| 热点网卡 IPv4，Windows 通常 `192.168.137.1` | 设备连接电脑移动热点时使用 |
| 电脑路由器 Wi-Fi/以太网网卡 IPv4 | 设备接入对应路由器且互通时使用 |

以实际网卡地址为准。Windows 热点常显示为 `本地连接*` / `Local Area Connection*`，不能与 VMware、VirtualBox、普通 Hyper-V 私有交换机一起过滤。电脑加入设备配网热点获得的临时 IP 也不是日常服务地址。

```powershell
Get-NetIPAddress -AddressFamily IPv4
Get-NetTCPConnection -State Listen | Where-Object { $_.LocalPort -in 3080,3101 }
```

网页 `3080` 和设备接口 `3101` 独立；网页只绑定 localhost 不妨碍连接器开放局域网。不要为语音终端开放无关网页端口。

通过网页端口读取 `GET /plugins/xiaomi-mimo-tts/passport-status`，关注 `listening`、`connected`、`servicesReady`、`recognitionReady`、`host`、`port`、`addresses`、`error`。ASR 冷启动中可能暂时未就绪，结合 provider 状态判断。

设备端点为认证的 `GET /api/health` 和 `ws://<电脑IP>:3101/api/realtime`，拒绝浏览器 Origin。电脑本机 health 成功只证明本机可达，不证明设备已穿过入站防火墙。

## 热点超时

HTTP 0、TCP 超时、握手前失败时，先核对地址、设备网段、热点隔离与入站规则。Node 版本管理器可能使用链接路径，要比较真实可执行路径；旧 Node 规则或仅专用网络规则未必覆盖当前热点。

已证实入站受限后，可在管理员 PowerShell 添加限定实际热点网段与端口的规则；下例只适用默认热点配置，先替换实际值：

```powershell
New-NetFirewallRule -DisplayName 'WhaleTalk Passport hotspot' -Direction Inbound -Action Allow -Profile Private,Public -Protocol TCP -LocalPort 3101 -LocalAddress 192.168.137.1 -RemoteAddress 192.168.137.0/24
```

复用同用途规则避免重复。不关闭整个防火墙、不修改无关规则、不绕过受管安全限制。改完观察实际设备连接，再验证认证和会话。
