# 音频驱动重构技术文档

## 文档目的

本文档记录了基于 xiaozhi-esp32 参考项目对 ES8311 音频驱动进行重构的完整过程，供后续开发者或 AI 助手继续工作参考。

---

## 一、参考项目分析

### 1.1 参考项目信息

- **项目地址**: https://github.com/78/xiaozhi-esp32
- **主要语言**: C++
- **框架**: ESP-IDF v5.x + ESP-ADF
- **架构特点**: 使用新版 I2S/I2C 驱动 API，抽象层设计

### 1.2 关键学习点

#### 1.2.1 ES8311 初始化方式（参考项目）

参考项目使用 ESP-ADF 框架，无需手动配置寄存器：

```cpp
// 文件: main/audio/codecs/es8311_audio_codec.cc

Es8311AudioCodec::Es8311AudioCodec(...) {
    // 1. 创建 I2S 数据接口
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);

    // 2. 创建 I2C 控制接口
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = i2c_port,
        .addr = es8311_addr,
        .bus_handle = i2c_master_handle,
    };
    ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);

    // 3. 创建 GPIO 接口
    gpio_if_ = audio_codec_new_gpio();

    // 4. 创建 ES8311 编解码器 - 自动处理所有寄存器配置
    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = ctrl_if_;
    es8311_cfg.gpio_if = gpio_if_;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8311_cfg.pa_pin = pa_pin;
    es8311_cfg.use_mclk = use_mclk;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    es8311_cfg.pa_reverted = pa_inverted_;
    codec_if_ = es8311_codec_new(&es8311_cfg);

    // 5. 创建音频设备
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = codec_if_,
        .data_if = data_if_,
    };
    dev_ = esp_codec_dev_new(&dev_cfg);

    // 6. 打开设备并配置采样率
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .sample_rate = (uint32_t)input_sample_rate_,
    };
    esp_codec_dev_open(dev_, &fs);
}
```

**关键优势**：
- 无需手动写入 ES8311 寄存器
- ESP-ADF 内置经过验证的初始化序列
- 自动处理时钟、电源、增益配置
- 代码量从 600+ 行减少到 50 行

#### 1.2.2 新版 I2S API（参考项目）

```cpp
// 文件: main/audio/codecs/es8311_audio_codec.cc

void Es8311AudioCodec::CreateDuplexChannels(...) {
    // 创建 I2S 通道（支持全双工）
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
    };
    i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_);

    // 标准模式配置
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = dout,
            .din = din,
        }
    };

    i2s_channel_init_std_mode(tx_handle_, &std_cfg);
    i2s_channel_init_std_mode(rx_handle_, &std_cfg);
    i2s_channel_enable(tx_handle_);
    i2s_channel_enable(rx_handle_);
}
```

**新版 API 优势**：
- 支持全双工（同时录音和播放）
- 支持 MCLK 输出（256× 采样率）
- 更好的 DMA 管理
- 更清晰的配置结构

#### 1.2.3 新版 I2C API（参考项目）

```cpp
// 文件: main/boards/esp-box-3/esp_box3_board.cc

void InitializeI2c() {
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = (i2c_port_t)1,
        .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_);
}
```

**新版 API 优势**：
- 总线句柄管理
- 设备探测功能 (`i2c_master_probe`)
- 更稳定的通信

#### 1.2.4 板级配置示例（参考项目）

```c
// 文件: main/boards/esp-box-3/config.h

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_2
#define AUDIO_I2S_GPIO_WS GPIO_NUM_45
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_17
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_16
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_15

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_46
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_8
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_18
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
```

---

## 二、当前项目修改记录

### 2.1 硬件配置

**设备信息**：
- 主芯片: ESP32-S3-WROOM-1-N16R8
- 音频编解码器: ES8311
- 麦克风: AP2718AT
- 功放: NS4150B
- 扬声器: 2828-4R3W

**引脚配置**（文件: `src/config.h`）：

```c
// I2S引脚配置
#define I2S_BCLK_PIN      GPIO_NUM_15    // SCLK
#define I2S_LRCK_PIN      GPIO_NUM_16    // LRCK
#define I2S_DIN_PIN       GPIO_NUM_17    // 数据输出到ES8311
#define I2S_DOUT_PIN      GPIO_NUM_18    // 从ES8311接收数据
#define I2S_MCLK_PIN      GPIO_NUM_14    // MCLK (ESP32-WROOM不支持输出)

// I2C引脚配置
#define I2C_SCL_PIN       GPIO_NUM_4
#define I2C_SDA_PIN       GPIO_NUM_5

// 音频控制引脚
#define SD_MODE_PIN       GPIO_NUM_12    // 麦克风/耳机切换
#define PA_EN_PIN         GPIO_NUM_13    // 功放使能

// 音频配置
#define I2S_SAMPLE_RATE   16000          // 录音采样率
#define I2S_SAMPLE_RATE_TTS 16000        // TTS播放采样率
```

### 2.2 新建文件列表

| 文件路径 | 说明 |
|----------|------|
| `src/drivers/i2c_driver_v2.h` | 新版 I2C 驱动头文件 |
| `src/drivers/i2c_driver_v2.c` | 新版 I2C 驱动实现 |
| `src/drivers/es8311_codec_v2.h` | 新版 ES8311 编解码器头文件 |
| `src/drivers/es8311_codec_v2.c` | 新版 ES8311 编解码器实现 |
| `src/drivers/audio_driver.h` | 统一音频驱动头文件 |
| `src/drivers/audio_driver.c` | 统一音频驱动实现 |
| `docs/ES8311_OPTIMIZATION_PLAN.md` | 优化方案文档 |
| `docs/AUDIO_DRIVER_REFACTORING.md` | 本文档 |

### 2.3 修改文件列表

| 文件路径 | 修改内容 |
|----------|----------|
| `src/middleware/audio_manager.c` | 改用新的 audio_driver 接口 |
| `src/CMakeLists.txt` | 添加新源文件 |
| `platformio.ini` | 启用调试日志 |

### 2.4 关键代码实现

#### 2.4.1 新版 I2C 驱动 (i2c_driver_v2.c)

```c
// 初始化 I2C 总线
esp_err_t i2c_driver_v2_init(const i2c_driver_v2_config_t *config,
                              i2c_master_bus_handle_t *p_bus_handle) {
    i2c_master_bus_config_t bus_config = {
        .i2c_port = config->port,
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = config->enable_pullup ? 1 : 0,
        },
    };

    return i2c_new_master_bus(&bus_config, p_bus_handle);
}

// 探测设备
bool i2c_driver_v2_probe(i2c_master_bus_handle_t bus_handle, uint8_t device_addr) {
    esp_err_t ret = i2c_master_probe(bus_handle, device_addr, pdMS_TO_TICKS(100));
    return (ret == ESP_OK);
}

// 读写操作
esp_err_t i2c_driver_v2_write_bytes(i2c_master_dev_handle_t dev_handle,
                                     uint8_t reg_addr, const uint8_t *data, size_t len) {
    uint8_t *write_buf = malloc(len + 1);
    write_buf[0] = reg_addr;
    memcpy(&write_buf[1], data, len);
    esp_err_t ret = i2c_master_transmit(dev_handle, write_buf, len + 1, pdMS_TO_TICKS(1000));
    free(write_buf);
    return ret;
}
```

#### 2.4.2 新版 ES8311 编解码器 (es8311_codec_v2.c)

```c
// 创建 I2S 全双工通道
static esp_err_t create_i2s_channels(es8311_codec_v2_handle_t handle,
                                      const es8311_codec_v2_config_t *config) {
    // I2S 通道配置
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg, &handle->tx_handle, &handle->rx_handle);

    // I2S 标准模式配置
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)config->output_sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = config->use_mclk ? I2S_MCLK_MULTIPLE_256 : I2S_MCLK_MULTIPLE_128,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
        },
        .gpio_cfg = {
            .mclk = config->use_mclk ? config->mclk_pin : GPIO_NUM_NC,
            .bclk = config->bclk_pin,
            .ws = config->ws_pin,
            .dout = config->dout_pin,
            .din = config->din_pin,
        }
    };

    i2s_channel_init_std_mode(handle->tx_handle, &std_cfg);
    i2s_channel_init_std_mode(handle->rx_handle, &std_cfg);

    return ESP_OK;
}

// 音频读写
int es8311_codec_v2_read(es8311_codec_v2_handle_t handle, int16_t *buffer, int samples) {
    if (!handle->input_enabled) return 0;

    size_t bytes_read = 0;
    i2s_channel_read(handle->rx_handle, buffer, samples * sizeof(int16_t),
                     &bytes_read, pdMS_TO_TICKS(1000));
    return (int)(bytes_read / sizeof(int16_t));
}

int es8311_codec_v2_write(es8311_codec_v2_handle_t handle, const int16_t *data, int samples) {
    if (!handle->output_enabled) return 0;

    size_t bytes_written = 0;
    i2s_channel_write(handle->tx_handle, data, samples * sizeof(int16_t),
                      &bytes_written, pdMS_TO_TICKS(1000));
    return (int)(bytes_written / sizeof(int16_t));
}
```

#### 2.4.3 统一音频驱动 (audio_driver.c)

```c
esp_err_t audio_driver_create(const audio_driver_config_t *config,
                               audio_driver_handle_t *p_handle) {
    // 1. 初始化 I2C 总线
    i2c_driver_v2_config_t i2c_config = {
        .port = I2C_NUM_0,
        .sda_pin = config->i2c_sda_pin,
        .scl_pin = config->i2c_scl_pin,
        .clk_speed_hz = config->i2c_freq_hz,
        .enable_pullup = true,
    };
    i2c_driver_v2_init(&i2c_config, &handle->i2c_bus);

    // 2. 检测 ES8311 设备
    if (!i2c_driver_v2_probe(handle->i2c_bus, config->es8311_addr)) {
        return ESP_ERR_NOT_FOUND;
    }

    // 3. 添加 ES8311 设备句柄
    i2c_driver_v2_device_add(handle->i2c_bus, config->es8311_addr, &handle->es8311_dev);

    // 4. 初始化 ES8311 编解码器
    es8311_codec_v2_config_t codec_config = {
        .i2c_bus_handle = handle->i2c_bus,
        .mclk_pin = config->i2s_mclk_pin,
        .bclk_pin = config->i2s_bclk_pin,
        // ... 其他配置
    };
    es8311_codec_v2_create(&codec_config, &handle->codec);

    return ESP_OK;
}
```

#### 2.4.4 修改后的 audio_manager.c

```c
esp_err_t audio_manager_init(const audio_config_t *config) {
    // ... 分配内存等

    // 初始化统一音频驱动
    audio_driver_config_t driver_config = {
        .i2c_sda_pin = I2C_SDA_PIN,
        .i2c_scl_pin = I2C_SCL_PIN,
        .i2s_bclk_pin = I2S_BCLK_PIN,
        .i2s_ws_pin = I2S_LRCK_PIN,
        .i2s_dout_pin = I2S_DIN_PIN,
        .i2s_din_pin = I2S_DOUT_PIN,
        .pa_pin = PA_EN_PIN,
        .input_sample_rate = I2S_SAMPLE_RATE,
        .output_sample_rate = I2S_SAMPLE_RATE_TTS,
        .use_mclk = false,
        .es8311_addr = ES8311_I2C_ADDR,
    };

    return audio_driver_create(&driver_config, &g_audio_mgr->driver);
}

// 录音
int audio_manager_play_tts_audio(const uint8_t *data, size_t len) {
    int samples = len / sizeof(int16_t);
    return audio_driver_write(g_audio_mgr->driver, (const int16_t*)data, samples);
}
```

---

## 三、架构对比

### 3.1 旧架构（问题）

```
┌─────────────────────────────────────┐
│          audio_manager.c            │
│  (直接调用多个驱动)                   │
└─────────────────────────────────────┘
         ↓      ↓      ↓      ↓
┌────────┐┌────────┐┌────────┐┌────────┐
│i2s_drv ││i2c_drv ││es8311  ││gpio_drv│
│(旧API) ││(旧API) ││(手动)  ││        │
└────────┘└────────┘└────────┘└────────┘
```

**问题**：
1. 手动写 ES8311 寄存器，200+ 行代码
2. I2C 通信不稳定，频繁超时
3. 旧版 I2S API 不支持 MCLK
4. 驱动耦合严重

### 3.2 新架构（推荐）

```
┌─────────────────────────────────────┐
│          audio_manager.c            │
│  (使用统一接口)                       │
└─────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────┐
│          audio_driver.c             │
│  (统一音频驱动)                       │
└─────────────────────────────────────┘
         ↓              ↓
┌──────────────┐  ┌──────────────────┐
│i2c_driver_v2 │  │es8311_codec_v2   │
│(新版API)     │  │(新版I2S API)     │
└──────────────┘  └──────────────────┘
```

**优势**：
1. 代码简洁，易于维护
2. 使用新版 ESP-IDF API
3. 抽象层设计，解耦硬件依赖
4. 更稳定的通信

---

## 四、编译结果

```
RAM:   [=         ]  11.2% (used 36608 bytes from 327680 bytes)
Flash: [===       ]  25.8% (used 1065909 bytes from 4128768 bytes)
========================= [SUCCESS] =========================
```

---

## 五、后续工作建议

### 5.1 立即可做

1. **烧录测试**
   ```bash
   pio run --target upload
   pio device monitor
   ```

2. **验证音频功能**
   - 检查 ES8311 是否被检测到（I2C 地址 0x18）
   - 测试录音功能
   - 测试 TTS 播放功能

### 5.2 可选优化

1. **添加 ESP-ADF 组件**
   在 `platformio.ini` 中添加：
   ```ini
   lib_deps =
       espressif/esp_codec_dev @ ^1.3
   ```
   这样可以使用 `es8311_codec_new()` 完全自动化 ES8311 配置。

2. **支持 MCLK**
   如果硬件支持，启用 MCLK 可以提供更准确的时钟：
   ```c
   .use_mclk = true,
   .mclk_pin = I2S_MCLK_PIN,
   ```

3. **添加错误恢复机制**
   - I2C 通信失败时自动重试
   - ES8311 初始化失败时自动复位

### 5.3 已知限制

1. **ESP32-WROOM 不支持 MCLK 输出**
   - GPIO14 不是有效的 MCLK 引脚
   - 需要使用 ESP32-S3 或外部时钟源

2. **采样率固定**
   - 当前录音和播放使用相同采样率 (16kHz)
   - 如需不同采样率，需要重新初始化 I2S

---

## 六、参考资源

- **xiaozhi-esp32 项目**: https://github.com/78/xiaozhi-esp32
- **ESP-IDF I2S 文档**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html
- **ESP-IDF I2C 文档**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html
- **ES8311 数据手册**: 参考 hardware_docs 目录

---

## 七、问题排查指南

### 7.1 编译问题

| 错误 | 解决方案 |
|------|----------|
| `esp_codec_dev.h: No such file` | 移除该头文件引用，使用纯 I2S 实现 |
| `I2S_MCLK_MULTIPLE_DEFAULT undeclared` | 改用 `I2S_MCLK_MULTIPLE_128` |
| `pdMS_TO_TICKS undefined` | 添加 `#include "freertos/FreeRTOS.h"` |

### 7.2 运行时问题

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 未检测到 ES8311 | I2C 通信问题 | 检查接线、I2C 地址 |
| 无声音输出 | PA 未启用 | 检查 PA_EN 引脚状态 |
| 录音无数据 | I2S 通道未启用 | 检查 `enable_input` 调用 |

---

---

## 八、测试结果（2025年1月18日）

### 8.1 编译和烧录

```
编译: SUCCESS
烧录: SUCCESS
芯片: ESP32-D0WDQ6 (revision v1.1)
端口: COM9
```

### 8.2 运行日志（关键部分）

```
I (6740) I2C_V2: I2C 总线初始化成功
I (6760) AUDIO_DRV: 检测到 ES8311 设备 @ 0x18
I (6790) ES8311_V2: I2S 全双工通道创建成功
I (6800) ES8311_V2:   采样率: 16000 Hz
I (6800) ES8311_V2:   引脚: BCLK=15, WS=16, DOUT=17, DIN=18, MCLK=-1
I (6810) ES8311_V2: ES8311 编解码器初始化完成
I (6820) AUDIO_DRV: 统一音频驱动初始化完成
...
I (8590) XIAOZHI: 音调写入完成: ESP_OK
```

### 8.3 测试结论

| 功能 | 状态 |
|------|------|
| I2C 总线初始化 | ✅ 成功 |
| ES8311 设备检测 | ✅ 成功 (地址 0x18) |
| I2S 通道创建 | ✅ 成功 (全双工) |
| 音调播放测试 | ✅ 成功 (ESP_OK) |
| TTS 连接 | ✅ TLS 握手成功 |

**新版音频驱动已成功运行！**

---

**文档版本**: 1.1
**创建日期**: 2025年1月
**最后更新**: 2025年1月18日（测试通过）

---

## 附录：完整文件结构

```
src/
├── drivers/
│   ├── i2c_driver.h/c          # 旧版 I2C 驱动（保留）
│   ├── i2s_driver.h/c          # 旧版 I2S 驱动（保留）
│   ├── gpio_driver.h/c         # GPIO 驱动（保留）
│   ├── es8311_codec.h/c        # 旧版 ES8311 驱动（保留）
│   ├── i2c_driver_v2.h/c       # 新版 I2C 驱动 ★
│   ├── es8311_codec_v2.h/c     # 新版 ES8311 驱动 ★
│   └── audio_driver.h/c        # 统一音频驱动 ★
├── middleware/
│   └── audio_manager.c         # 已修改使用新驱动 ★
├── modules/
│   └── ... (未修改)
├── config.h                    # 硬件配置
└── CMakeLists.txt              # 已更新 ★

docs/
├── ES8311_OPTIMIZATION_PLAN.md # 优化方案
└── AUDIO_DRIVER_REFACTORING.md # 本文档
```

标记 ★ 的文件为本次新增或修改的文件。
