# ESP32-S3 XiaoZhi Voice Assistant

基于 `ESP32-S3` 的智能语音助手固件，集成离线唤醒、在线语音识别、大模型对话、语音合成和 `1.28"` 圆形 TFT 显示界面。

当前工程采用 `PlatformIO + ESP-IDF 5.5` 构建，目标硬件为 `ESP32-S3 N16R8 + ES8311 + GC9A01`。系统主链路面向真实设备运行场景做了收敛，重点保证语音交互、显示联动和长期可维护性。

## 项目介绍

本项目实现了一套完整的端侧语音助手流程：

1. `WakeNet` 本地检测唤醒词“你好小智”
2. 录制语音并通过科大讯飞 WebSocket ASR 识别文本
3. 将识别结果发送给 `DeepSeek` 生成回答
4. 通过科大讯飞 WebSocket TTS 合成语音并播报
5. 在 `GC9A01` 圆形屏幕上同步显示时间、状态、用户提问和模型回复

当前 UI 已支持以下状态：

- 启动中
- 待唤醒时钟页
- 聆听中
- 思考中
- 播报中
- 错误提示

交互层已实现连续会话窗口：唤醒后完成一次问答后，设备会继续保留一段上下文时间，用户无需再次说唤醒词即可继续提问。

## 主要功能

- 离线唤醒：`ESP-SR WakeNet`
- 在线 ASR：`iFlytek WebSocket`
- 大模型对话：`DeepSeek Chat Completions`
- 在线 TTS：`iFlytek WebSocket`
- 音频编解码：`ES8311`
- 圆屏显示：`GC9A01 240x240 SPI`
- 中文文本渲染：内置常用字模，支持常见中文回复显示
- 构建系统：`PlatformIO`

## 软件架构

```text
src/
├── app/
│   └── app_main.c                 # 系统主流程与状态编排
├── config/
│   └── app_config.h               # 所有核心参数、GPIO 与云端配置
├── bsp/
│   ├── audio/                     # 音频板级驱动、ES8311 适配
│   ├── bus/                       # I2C 封装
│   └── display/                   # GC9A01 面板驱动
└── services/
    ├── audio/                     # 录音、播放、缓冲管理
    ├── cloud/                     # DeepSeek 对话
    ├── display/                   # UI 渲染、中文字体、界面状态
    ├── network/                   # Wi-Fi
    └── speech/                    # WakeNet、ASR、TTS
```

核心运行链路：

```text
WakeNet -> ASR -> Chat -> TTS -> Speaker
                 \-> TFT UI status/text
```

## 环境要求

- Windows / macOS / Linux
- Python `3.10+`
- [PlatformIO Core](https://platformio.org/install)
- USB 转串口驱动
- 目标开发板：`ESP32-S3 N16R8`

当前默认构建环境：

```ini
[platformio]
default_envs = esp32-s3-n16r8
```

## 安装教程

### 1. 克隆仓库

```powershell
git clone https://github.com/lighttravel/GraduateWork.git
cd GraduateWork
git checkout codex/esp32s3
```

### 2. 安装 PlatformIO Core

参考官方文档安装：

- [PlatformIO Core installation](https://docs.platformio.org/en/latest/core/installation/index.html)

安装完成后确认：

```powershell
pio --version
```

### 3. 构建固件

```powershell
pio run -e esp32-s3-n16r8
```

### 4. 烧录到开发板

将串口号替换为你的实际端口，例如 `COM12`：

```powershell
pio run -e esp32-s3-n16r8 -t upload --upload-port COM12
```

### 5. 打开串口日志

```powershell
pio device monitor -p COM12 -b 115200
```

建议关注以下启动日志关键字：

- `waiting wake word`
- `Wake word detected`
- `ASR final`
- `chat done`
- `TTS done`

## 参数配置指南

所有核心参数集中定义在 [`src/config/app_config.h`](src/config/app_config.h)。

### 1. 硬件引脚配置

#### 音频接口

| 宏 | 默认值 | 说明 |
| --- | --- | --- |
| `I2S_MCLK_PIN` | `GPIO6` | ES8311 主时钟 |
| `I2S_BCLK_PIN` | `GPIO14` | I2S Bit Clock |
| `I2S_LRCK_PIN` | `GPIO12` | I2S 左右声道时钟 |
| `I2S_DIN_PIN` | `GPIO11` | ESP32 -> ES8311 播放数据 |
| `I2S_DOUT_PIN` | `GPIO13` | ES8311 -> ESP32 录音数据 |
| `I2C_SCL_PIN` | `GPIO4` | ES8311 I2C 时钟 |
| `I2C_SDA_PIN` | `GPIO5` | ES8311 I2C 数据 |
| `ES8311_I2C_ADDR` | `0x18` | 编解码器 I2C 地址 |
| `LED_STATUS_PIN` | `GPIO47` | 状态灯 |

#### TFT 显示接口

| TFT 引脚 | 宏 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `SCL/SCK` | `TFT_SPI_SCLK_PIN` | `GPIO15` | SPI 时钟 |
| `SDA/MOSI` | `TFT_SPI_MOSI_PIN` | `GPIO16` | SPI 数据 |
| `DC` | `TFT_DC_PIN` | `GPIO17` | 数据/命令选择 |
| `CS` | `TFT_CS_PIN` | `GPIO18` | 片选 |
| `RES/RST` | `TFT_RST_PIN` | `GPIO7` | 复位 |
| `BLK` | `TFT_BL_PIN` | `GPIO21` | 背光控制 |
| 屏幕宽度 | `TFT_H_RES` | `240` | 像素宽度 |
| 屏幕高度 | `TFT_V_RES` | `240` | 像素高度 |

#### 屏幕方向与显示参数

| 宏 | 默认值 | 说明 |
| --- | --- | --- |
| `TFT_MIRROR_X` | `true` | 左右镜像 |
| `TFT_MIRROR_Y` | `false` | 上下镜像 |
| `TFT_SWAP_XY` | `false` | 交换 X/Y 轴 |
| `TFT_INVERT_COLOR` | `true` | 色彩反转 |
| `TFT_SPI_PCLK_HZ` | `40 * 1000 * 1000` | SPI 时钟频率 |

如果屏幕方向不对，优先只调整以上 3 个方向宏，不要分散改驱动层代码。

### 2. 音频参数

| 宏 | 默认值 | 说明 |
| --- | --- | --- |
| `I2S_SAMPLE_RATE` | `16000` | 录音采样率 |
| `I2S_SAMPLE_RATE_TTS` | `16000` | TTS 播放采样率 |
| `I2S_BITS_PER_SAMPLE` | `16bit` | 位宽 |
| `I2S_DMA_BUF_COUNT` | `8` | DMA 缓冲块数 |
| `I2S_DMA_BUF_LEN` | `512` | DMA 单块长度 |
| `AUDIO_VOLUME` | `55` | 播放音量，范围建议 `0-100` |
| `AUDIO_INPUT_GAIN_DB` | `30.0f` | 麦克风输入增益 |
| `VAD_THRESHOLD` | `100` | 语音活动检测阈值 |

调参建议：

- 识别不灵敏：适度提高 `AUDIO_INPUT_GAIN_DB`
- 底噪较大：降低 `AUDIO_INPUT_GAIN_DB`
- 播放过爆：降低 `AUDIO_VOLUME`

### 3. 唤醒与交互时序

| 宏 | 默认值 | 说明 |
| --- | --- | --- |
| `APP_WAKE_WORD_MODEL` | `nihaoxiaozhi` | WakeNet 模型名 |
| `APP_WAKE_WORD_NAME` | `你好小智` | 唤醒词显示名 |
| `APP_WAKE_WORD_THRESHOLD` | `0.20f` | 唤醒阈值 |
| `APP_WAKE_WORD_VAD_THRESHOLD` | `0.20f` | 唤醒阶段 VAD 阈值 |
| `APP_ASR_RECORD_SECONDS` | `10` | 单轮录音时长 |
| `APP_ASR_FINAL_TIMEOUT_MS` | `12000` | ASR 最终结果超时 |
| `APP_CHAT_TIMEOUT_MS` | `30000` | 大模型回复超时 |
| `APP_TTS_TIMEOUT_MS` | `90000` | TTS 合成/播放超时 |

调参建议：

- 误唤醒较多：提高 `APP_WAKE_WORD_THRESHOLD`
- 唤醒不灵敏：适度降低 `APP_WAKE_WORD_THRESHOLD`
- 用户说话偏慢：提高 `APP_ASR_RECORD_SECONDS`

### 4. 文本和回复约束

| 宏 | 默认值 | 说明 |
| --- | --- | --- |
| `APP_REPLY_TEXT_MAX_LEN` | `1024` | 显示层缓存的回复文本长度 |
| `APP_TTS_REPLY_MAX_CHARS` | `120` | 送入 TTS 的最大字符数 |
| `APP_TTS_REPLY_MAX_SENTENCES` | `3` | 最多提取句子数 |
| `APP_CHAT_MAX_TOKENS` | `180` | DeepSeek 输出 token 上限 |
| `APP_CHAT_SYSTEM_PROMPT` | 内置英文提示词 | 约束回答风格 |
| `APP_FALLBACK_REPLY` | 中文兜底回复 | 云端失败时的默认文案 |

如果你希望回复更短、更口语化，优先修改 `APP_CHAT_SYSTEM_PROMPT` 和 `APP_CHAT_MAX_TOKENS`。

### 5. Wi-Fi 配置

| 宏 | 默认值 | 说明 |
| --- | --- | --- |
| `DEFAULT_WIFI_SSID` | `XUNTIAN_2.4G` | 默认 Wi-Fi 名称 |
| `DEFAULT_WIFI_PASSWORD` | 已配置 | 默认 Wi-Fi 密码 |
| `WIFI_AP_SSID` | `ESP32-Xiaozhi` | AP 配网热点名 |
| `WIFI_CONNECT_TIMEOUT_MS` | `30000` | Wi-Fi 连接超时 |
| `WIFI_RETRY_COUNT` | `5` | 重试次数 |

部署前建议替换默认 Wi-Fi 凭据，避免测试信息进入正式环境。

### 6. 云端服务参数

| 类别 | 关键宏 |
| --- | --- |
| 讯飞 ASR | `IFLYTEK_APPID` `IFLYTEK_API_KEY` `IFLYTEK_API_SECRET` `IFLYTEK_ASR_LANGUAGE` `IFLYTEK_ASR_DOMAIN` |
| 讯飞 TTS | `IFLYTEK_TTS_VOICE` `IFLYTEK_TTS_SPEED` `IFLYTEK_TTS_REQUEST_VOLUME` `IFLYTEK_TTS_PITCH` |
| DeepSeek | `DEEPSEEK_API_URL` `DEEPSEEK_API_HOST` `DEEPSEEK_API_PORT` `DEEPSEEK_API_KEY` `DEEPSEEK_MODEL` |

强烈建议：

- 发布前替换所有测试用 `API Key`
- 不要把正式密钥长期硬编码在仓库中
- 生产环境应改为私有配置文件、NVS 或安全存储

### 7. 任务与内存参数

| 宏 | 默认值 | 说明 |
| --- | --- | --- |
| `SYSTEM_TASK_STACK_SIZE` | `8 * 1024` | 系统任务栈 |
| `AUDIO_TASK_STACK_SIZE` | `12 * 1024` | 音频任务栈 |
| `HTTP_TASK_STACK_SIZE` | `4 * 1024` | HTTP 任务栈 |
| `MAX_CHAT_HISTORY` | `6` | 对话上下文条数 |
| `MAX_MESSAGE_LEN` | `1024` | 单条消息上限 |
| `MAX_RESPONSE_LEN` | `2048` | 单次回复缓冲上限 |

## 硬件连接说明

### 1. 最小系统连接

| 模块 | 连接要求 |
| --- | --- |
| ESP32-S3 | 稳定 `3.3V` 供电 |
| ES8311 | 与 ESP32-S3 共地 |
| GC9A01 屏幕 | 与 ESP32-S3 共地，共用 `3.3V` 逻辑电平 |
| 功放/喇叭 | 参考板级设计接入 |

注意事项：

- `GND` 必须共地，否则可能出现屏幕黑屏、供电电压异常、SPI 初始化正常但无显示等现象
- 屏幕和音频模块均应使用 `3.3V` 逻辑
- `BLK` 可接 `GPIO21` 由固件控制，也可在硬件验证阶段直接上拉到 `3.3V`

### 2. ES8311 音频连接

| ES8311/音频信号 | ESP32-S3 |
| --- | --- |
| `MCLK` | `GPIO6` |
| `BCLK` | `GPIO14` |
| `LRCK` | `GPIO12` |
| `DACDIN` | `GPIO11` |
| `ADCDOUT` | `GPIO13` |
| `SCL` | `GPIO4` |
| `SDA` | `GPIO5` |

### 3. GC9A01 圆屏连接

| TFT Pin | ESP32-S3 Pin | 说明 |
| --- | --- | --- |
| `VCC` | `3.3V` | 电源 |
| `GND` | `GND` | 地 |
| `SCL/SCK` | `GPIO15` | SPI 时钟 |
| `SDA/MOSI` | `GPIO16` | SPI 数据 |
| `DC` | `GPIO17` | 数据/命令 |
| `CS` | `GPIO18` | 片选 |
| `RES/RST` | `GPIO7` | 复位 |
| `BLK` | `GPIO21` | 背光 |

## 使用流程

1. 上电启动
2. 连接 Wi-Fi
3. 同步 SNTP 时间
4. 初始化 ES8311、WakeNet、TFT 屏幕
5. 显示待机时钟页并等待唤醒词
6. 检测到“你好小智”后进入聆听界面
7. 识别用户提问并显示文本
8. 拉取模型回复并在屏幕上流式显示
9. 进入播报状态播放语音
10. 保留连续对话窗口，等待下一轮提问

## 常见问题排查

### 屏幕不亮

- 检查 `VCC-GND` 是否稳定为 `3.3V`
- 检查 `GND` 是否确实共地
- 检查 `BLK` 是否接到 `GPIO21` 或直接上拉 `3.3V`
- 检查 `TFT_MIRROR_X / TFT_MIRROR_Y / TFT_SWAP_XY` 是否误配

### 屏幕内容镜像或旋转方向不对

只调整以下参数：

- `TFT_MIRROR_X`
- `TFT_MIRROR_Y`
- `TFT_SWAP_XY`

### 唤醒不稳定

- 调整 `APP_WAKE_WORD_THRESHOLD`
- 调整 `APP_WAKE_WORD_VAD_THRESHOLD`
- 结合现场噪声调整 `AUDIO_INPUT_GAIN_DB`

### 语音识别结果差

- 检查麦克风硬件和 ES8311 连接
- 适度提高输入增益
- 缩短环境回声路径，避免喇叭串入麦克风

### 云端接口失败

- 检查 Wi-Fi 是否正常联网
- 检查 SNTP 时间是否同步成功
- 检查 `IFLYTEK_*` 和 `DEEPSEEK_*` 参数是否有效

## 构建说明

当前仓库默认使用 PlatformIO：

```powershell
pio run -e esp32-s3-n16r8
```

如果需要清理：

```powershell
pio run -e esp32-s3-n16r8 -t clean
```

## 安全说明

当前仓库中的 `Wi-Fi` 和云服务参数包含测试用途配置。用于演示和联调时可以直接运行，但用于公开部署或生产设备前，必须至少完成以下处理：

- 替换默认 Wi-Fi 信息
- 替换 `IFLYTEK` 凭据
- 替换 `DEEPSEEK` API Key
- 将敏感信息迁移到更安全的配置方式

## 适用对象

本项目适合以下场景：

- 毕业设计或课程设计演示
- ESP32-S3 语音交互原型机
- 圆屏语音助手终端
- WakeNet + ASR + LLM + TTS 的端云协同实验平台
