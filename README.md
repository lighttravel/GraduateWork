# ESP32-S3 XiaoZhi Voice Assistant

一个面向 ESP32-S3 + ES8311 的精简语音助手固件。项目只保留一条可运行主链路：

`WakeNet 唤醒 -> 讯飞 ASR -> DeepSeek Chat -> 讯飞 TTS -> 扬声器播放`

目标是让工程适合长期维护，而不是继续堆叠实验代码。

## Features

- 离线唤醒：`ESP-SR WakeNet`
- 在线语音识别：`iFlytek WebSocket ASR`
- 大模型对话：`DeepSeek Chat API`
- 在线语音合成：`iFlytek WebSocket TTS`
- 音频编解码：`ES8311 官方驱动 + ESP-IDF I2S`
- 构建系统：`PlatformIO + ESP-IDF`

## Project Layout

```text
AIchatesp32/
├── components/                  # 第三方组件（ESP-SR / ES8311 / websocket 等）
├── src/
│   ├── app/
│   │   └── app_main.c           # 主状态机：Wake -> ASR -> Chat -> TTS
│   ├── config/
│   │   └── app_config.h         # 所有可调参数集中配置
│   ├── bsp/
│   │   ├── audio/
│   │   │   ├── audio_board.c    # 板级音频入口
│   │   │   ├── audio_board.h
│   │   │   ├── es8311_codec.c   # ES8311 + I2S 封装
│   │   │   └── es8311_codec.h
│   │   └── bus/
│   │       ├── i2c_bus.c        # I2C 总线封装
│   │       └── i2c_bus.h
│   ├── services/
│   │   ├── audio/
│   │   │   ├── audio_service.c  # 录音/播放服务
│   │   │   └── audio_service.h
│   │   ├── cloud/
│   │   │   ├── chat_service.c   # DeepSeek 对话
│   │   │   └── chat_service.h
│   │   ├── network/
│   │   │   ├── wifi_service.c   # Wi-Fi 连接
│   │   │   └── wifi_service.h
│   │   └── speech/
│   │       ├── wake_word_service.c
│   │       ├── wake_word_service.h
│   │       ├── asr_service.c
│   │       ├── asr_service.h
│   │       ├── tts_service.c
│   │       ├── tts_service.h
│   │       ├── iflytek_tts_client.c
│   │       └── iflytek_tts_client.h
│   └── CMakeLists.txt
├── partitions.csv               # 分区表，含 model 分区
├── platformio.ini               # PlatformIO 入口
├── sdkconfig.defaults
├── sdkconfig.esp32-s3-n16r8
└── README.md
```

## Hardware

### Supported Board

- MCU: `ESP32-S3 N16R8`
- Audio codec: `ES8311`
- Amplifier: `NS4150B` or compatible power amp

### Pin Mapping

所有硬件相关参数都集中在 [`src/config/app_config.h`](/E:/graduatework/AIchatesp32/src/config/app_config.h)。

| Signal | ESP32-S3 GPIO | 说明 |
| --- | --- | --- |
| `I2S_MCLK_PIN` | `GPIO6` | ES8311 主时钟 |
| `I2S_BCLK_PIN` | `GPIO14` | I2S 位时钟 |
| `I2S_LRCK_PIN` | `GPIO12` | I2S 左右声道时钟 |
| `I2S_DIN_PIN` | `GPIO11` | ESP32 -> ES8311 播放数据 |
| `I2S_DOUT_PIN` | `GPIO13` | ES8311 -> ESP32 录音数据 |
| `I2C_SCL_PIN` | `GPIO4` | ES8311 控制总线时钟 |
| `I2C_SDA_PIN` | `GPIO5` | ES8311 控制总线数据 |
| `LED_STATUS_PIN` | `GPIO47` | 状态指示灯 |

如果你的板子接线不同，只改 `app_config.h` 里的 GPIO 宏，不要到各个模块里散改。

## Build And Flash

### 1. Install

- 安装 [PlatformIO Core](https://platformio.org/install)
- 安装 Python 3.10+
- 克隆仓库

```powershell
git clone https://github.com/lighttravel/GraduateWork.git
cd GraduateWork
git checkout codex/esp32s3
```

### 2. Build

```powershell
pio run -e esp32-s3-n16r8
```

### 3. Flash

```powershell
pio run -e esp32-s3-n16r8 -t upload --upload-port COM12
```

### 4. Monitor

```powershell
pio device monitor -p COM12 -b 115200
```

## Parameter Guide

所有核心参数都集中在 [`src/config/app_config.h`](/E:/graduatework/AIchatesp32/src/config/app_config.h)。

### GPIO 映射

修改以下宏即可：

- `I2S_MCLK_PIN`
- `I2S_BCLK_PIN`
- `I2S_LRCK_PIN`
- `I2S_DIN_PIN`
- `I2S_DOUT_PIN`
- `I2C_SCL_PIN`
- `I2C_SDA_PIN`
- `AUDIO_PA_ENABLE_PIN`

### 音量

修改：

```c
#define AUDIO_VOLUME 55
```

说明：

- 范围 `0-100`
- 偏大就调低
- 有爆音先降低再看功放电源和扬声器

### 麦克风增益

修改：

```c
#define AUDIO_INPUT_GAIN_DB 30.0f
```

说明：

- 识别不灵敏就逐步提高
- 底噪明显就降低
- 建议每次改动不超过 `3 dB`

### 唤醒灵敏度

修改：

```c
#define APP_WAKE_WORD_THRESHOLD     0.20f
#define APP_WAKE_WORD_VAD_THRESHOLD 0.20f
```

说明：

- 数值越低，越容易被唤醒
- 环境噪声大时要适当升高
- 当前模型关键词是：

```c
#define APP_WAKE_WORD_MODEL "nihaoxiaozhi"
#define APP_WAKE_WORD_NAME  "你好小智"
```

### Wi-Fi

修改：

```c
#define DEFAULT_WIFI_SSID     "your_ssid"
#define DEFAULT_WIFI_PASSWORD "your_password"
```

### 云端服务

修改以下宏即可切换账号：

- `IFLYTEK_APPID`
- `IFLYTEK_API_KEY`
- `IFLYTEK_API_SECRET`
- `DEEPSEEK_API_KEY`

### 交互时长

你可以直接调这些超时参数：

- `APP_ASR_RECORD_SECONDS`
- `APP_ASR_FINAL_TIMEOUT_MS`
- `APP_CHAT_TIMEOUT_MS`
- `APP_TTS_TIMEOUT_MS`

## Runtime Flow

1. 上电后连接 Wi-Fi
2. 同步 SNTP 时间，保证 TLS 鉴权稳定
3. 初始化 ES8311 / I2S / WakeNet
4. 等待唤醒词 `你好小智`
5. 录音并发送到讯飞 ASR
6. 把识别文本发给 DeepSeek
7. 把回复文本发给讯飞 TTS
8. 播放语音并回到待唤醒状态

## Serial Logs To Watch

- `waiting wake word`
- `Wake word detected`
- `ASR final: ...`
- `chat done: ...`
- `TTS done`

## Notes

- 工程已经删除大部分一次性测试入口，只保留主链路源码
- 当前默认环境只有 `esp32-s3-n16r8`
- `components/` 下是第三方依赖，不建议随意改动
