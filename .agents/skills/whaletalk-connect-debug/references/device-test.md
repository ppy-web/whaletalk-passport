# 构建身份与设备验收

目标 ESP32-C3 / 8 MB / 无 PSRAM，ESP-IDF 5.5.3。使用本仓库 setup/activate 和 validate，不复制开发者绝对路径。

```powershell
./tools/setup.ps1
. ./tools/activate.ps1
python tools/validate.py --static
python tools/validate.py
python tools/validate.py --preview
```

gate 保存同次合并镜像、ELF/MAP、哈希归档与 manifest。运行 `python tools/archive_firmware.py verify <归档目录>`，从 manifest 取文件名、目标、偏移与哈希。不交付失败构建遗留的旧镜像，不用不匹配 ELF 解码。

刷写前只读枚举，确认设备、镜像和数据影响，并核对本次授权。已明确覆盖的授权不重复询问；构建、USB 在线或旧版本刷写授权不能推定为新镜像写入授权。

- 完整镜像写 `0x0`，填充间隙可能覆盖 NVS；整片擦除不是默认步骤。
- 保留设置先证实当前分区/引导兼容，再按 manifest 写组件；本项目应用为 `0x10000`，应用镜像不能写 `0x0`。
- 保留 `cardid@0x356000`、`recovery@0x700000` 布局。合并镜像不含新 Recovery，不能承诺原恢复入口可用。
- 刷写成功不等于验收。分别测试界面、按键、转写、声音、两轮、取消、重连，记录未测项。

原固件备份不是刷写前置要求，是否保留设置是另一回事。原始日志、NVS、密码、配对码、DSH profile、固件/ELF 不进入源码 Git 或公共 issue。
