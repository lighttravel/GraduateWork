# 音频驱动快速参考卡

## 最新状态：✅ 测试通过（2025年1月18日）

新版音频驱动已成功运行：
- I2C 通信正常
- ES8311 设备检测成功 (0x18)
- I2S 全双工通道创建成功
- 测试音调播放成功

---

## 一、关键文件位置

```
新驱动:
├── src/drivers/audio_driver.h/c        # 统一入口
├── src/drivers/i2c_driver_v2.h/c       # 新版 I2C
├── src/drivers/es8311_codec_v2.h/c     # 新版 ES8311
└── src/middleware/audio_manager.c      # 已改用新驱动

文档:
├── docs/ES8311_OPTIMIZATION_PLAN.md    # 优化方案
├── docs/AUDIO_DRIVER_REFACTORING.md    # 完整技术文档
└── docs/QUICK_REFERENCE.md             # 本文档
```

## 二、核心 API 快速参考

### 2.1 统一音频驱动 (audio_driver.h)

```c
// 初始化
audio_driver_config_t config = {
    .i2c_sda_pin = GPIO_NUM_5,
    .i2c_scl_pin = GPIO_NUM_4,
    .i2s_bclk_pin = GPIO_NUM_15,
    .i2s_ws_pin = GPIO_NUM_16,
    .i2s_dout_pin = GPIO_NUM_17,
    .i2s_din_pin = GPIO_NUM_18,
    .pa_pin = GPIO_NUM_13,
    .input_sample_rate = 16000,
    .output_sample_rate = 16000,
    .use_mclk = false,
    .es8311_addr = 0x18,
};
audio_driver_create(&config, &handle);

// 录音
audio_driver_start_recording(handle);
int samples = audio_driver_read(handle, buffer, 1024);
audio_driver_stop_recording(handle);

// 播放
audio_driver_start_playback(handle);
int samples = audio_driver_write(handle, data, 1024);
audio_driver_stop_playback(handle);

// 控制
audio_driver_set_volume(handle, 80);
audio_driver_set_mute(handle, false);
```

### 2.2 audio_manager 接口（应用层使用）

```c
// 初始化
audio_config_t config = {
    .sample_rate = 16000,
    .volume = 80,
};
audio_manager_init(&config);

// TTS 播放
audio_manager_start_tts_playback();
audio_manager_play_tts_audio(data, len);
audio_manager_stop_tts_playback();

// 录音
audio_manager_start_record();
audio_manager_stop_record();
```

## 三、硬件配置 (config.h)

```c
// I2S
#define I2S_BCLK_PIN   GPIO_NUM_15
#define I2S_LRCK_PIN   GPIO_NUM_16
#define I2S_DIN_PIN    GPIO_NUM_17  // -> ES8311
#define I2S_DOUT_PIN   GPIO_NUM_18  // <- ES8311
#define I2S_MCLK_PIN   GPIO_NUM_14  // (不使用)

// I2C
#define I2C_SCL_PIN    GPIO_NUM_4
#define I2C_SDA_PIN    GPIO_NUM_5

// 控制
#define PA_EN_PIN      GPIO_NUM_13
#define SD_MODE_PIN    GPIO_NUM_12

// 音频
#define I2S_SAMPLE_RATE     16000
#define I2S_SAMPLE_RATE_TTS 16000
#define ES8311_I2C_ADDR     0x18
```

## 四、编译和烧录

```bash
# 编译
pio run -e esp32dev

# 烧录
pio run --target upload

# 串口监视
pio device monitor

# 一键操作
pio run --target upload && pio device monitor
```

## 五、问题排查

| 问题 | 检查 |
|------|------|
| ES8311 未检测到 | I2C 地址 0x18，检查 SDA/SCL 接线 |
| 无声音 | PA_EN 是否为高电平，检查功放接线 |
| 录音无数据 | SD_MODE 是否正确，检查麦克风接线 |
| I2C 超时 | 检查上拉电阻，降低 I2C 频率 |

## 六、参考项目

- **xiaozhi-esp32**: https://github.com/78/xiaozhi-esp32
- 关键文件: `main/audio/codecs/es8311_audio_codec.cc`
- 板配置示例: `main/boards/esp-box-3/`

## 七、新 vs 旧 API 对照

| 功能 | 旧 API | 新 API |
|------|--------|--------|
| I2S 初始化 | `i2s_driver_install()` | `i2s_new_channel()` |
| I2S 配置 | `i2s_set_pin()` | `i2s_channel_init_std_mode()` |
| I2S 读写 | `i2s_read/write()` | `i2s_channel_read/write()` |
| I2C 初始化 | `i2c_driver_install()` | `i2c_new_master_bus()` |
| I2C 读写 | `i2c_master_cmd_begin()` | `i2c_master_transmit/receive()` |

## 八、后续优化建议

1. **添加 ESP-ADF**: 可使用 `es8311_codec_new()` 完全自动化配置
2. **启用 MCLK**: 需要硬件支持（ESP32-S3）
3. **动态采样率**: 添加采样率切换功能

---

**创建日期**: 2025年1月
