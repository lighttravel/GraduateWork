# 路小班ES8311+NS4150B模块 - 实际接线指南

> **模块**: 路小班 ES8311 + NS4150B 音频模块
> **目标**: ESP32-S3-N16R8
> **特点**: 仅9个引脚,简化接线,无控制引脚

---

## 📌 模块实际引脚 (9个)

路小班模块只有以下9个引脚:

| 引脚标识 | 功能 | 方向 | 说明 |
|---------|------|------|------|
| **I2S接口** |||
| SCLK / BCLK | I2S位时钟 | ESP32→模块 | 通常为采样率×32 |
| LRCK | I2S字选择时钟 | ESP32→模块 | 采样频率(16/24/48kHz) |
| DIN | I2S数据输入 | ESP32→模块 | 播放音频数据 |
| DOUT | I2S数据输出 | 模块→ESP32 | 录音音频数据 |
| MCLK | I2S主时钟(可选) | ESP32→模块 | 采样率×256,推荐使用 |
| **I2C接口** |||
| SCL | I2C时钟 | ESP32→模块 | 100kHz标准速度 |
| SDA | I2C数据 | 双向 | ES8311地址0x18或0x19 |
| **电源** |||
| 5V | 电源正极 | - | **注意:是5V不是3.3V!** |
| GND | 地 | - | 与ESP32共地 |

### ⚠️ 重要注意事项

1. **模块电源是5V,不是3.3V!**
2. **无SD_MODE引脚** - 麦克风/耳机切换由ES8311内部自动处理或硬件检测
3. **无PA_EN引脚** - 功放开关由ES8311内部控制,无需外部控制
4. **I2C地址** - 通常为0x18(ADDPIN=GND),如果ADDPIN接VDD则为0x19

---

## 🔌 ESP32-S3接线方案

### 完整接线表

| 路小班模块引脚 | ESP32-S3引脚 | 颜色建议 | 功能说明 |
|--------------|------------|---------|---------|
| **I2S音频接口** |||
| SCLK / BCLK | GPIO15 | 🟡 黄色 | I2S位时钟 |
| LRCK | GPIO16 | 🟢 绿色 | I2S字选择时钟 |
| DIN | GPIO17 | 🔵 蓝色 | I2S数据输入 (播放) |
| DOUT | GPIO18 | 🟣 紫色 | I2S数据输出 (录音) |
| MCLK | GPIO38 | ⚪ 白色 | I2S主时钟 (推荐) |
| **I2C控制接口** |||
| SCL | GPIO8 | 🟠 橙色 | I2C时钟 |
| SDA | GPIO9 | 🟤 棕色 | I2C数据 |
| **电源** |||
| 5V | 5V | 🔴 红色(粗) | **5V电源(不是3.3V!)** |
| GND | GND | ⚫ 黑色(粗) | 共地 |

### 接线图

```
路小班ES8311+NS4150B模块          ESP32-S3开发板
┌─────────────────────┐       ┌─────────────────────────┐
│                     │       │                         │
│  ES8311 Codec       │       │  GPIO15 ────────────┐   │
│  ┌──────────────┐   │       │  GPIO16 ─────────┐  │   │
│  │    I2S       │   │       │  GPIO17 ──────┐  │  │   │
│  │              │   │       │  GPIO18 ───┐  │  │  │   │
│  │ SCLK ────────┼───┼──────┼── GPIO38 ─┐  │  │  │   │
│  │ LRCK ────────┼───┼──────┼─┐         │  │  │  │   │
│  │ DIN ─────────┼───┼──────┼─┼────┐    │  │  │  │   │
│  │ DOUT ────────┼───┼──────┼─┼───┐┼──┐ │  │  │   │
│  │ MCLK ────────┼───┼──────┼─┼───┼┼──┼─┼─┐│  │   │
│  │              │   │       │ │  ││  │ │ ││  │   │
│  └──────────────┘   │       │ │  ││  │ │ ││  │   │
│                     │       │ │  ││  │ │ ││  │   │
│  ┌──────────────┐   │       │ │  ││  │ │ ││  │   │
│  │    I2C       │   │       │ │  ││  │ │ ││  │   │
│  │              │   │       │ │  ││  │ │ ││  │   │
│  │ SCL ─────────┼───┼──────┼─┼──┼┼──┼─┼─┼─┼───┼─── GPIO8
│  │ SDA ─────────┼───┼──────┼─┼──┼┼──┼─┼─┼─┼───┼─── GPIO9
│  │              │   │       │ │  ││  │ │ ││  │   │
│  └──────────────┘   │       │ │  ││  │ │ ││  │   │
│                     │       │ │  ││  │ │ ││  │   │
│  ┌──────────────┐   │       │ │  ││  │ │ ││  │   │
│  │    Power     │   │       │ │  ││  │ │ ││  │   │
│  │              │   │       │ │  ││  │ │ ││  │   │
│  │  5V ─────────┼───┼──────┼─┼──┼┼──┼─┼─┼─┼───┼─── 5V ⚠️
│  │ GND ─────────┼───┼──────┼─┼──┼┼──┼─┼─┼─┼───┼─── GND
│  └──────────────┘   │       │ └──┴┼──┴─┴─┴─┼──┴───┘
└─────────────────────┘       └─────────────────┘

⚠️ 注意: 模块需要5V供电,不是3.3V!
```

---

## ⚙️ 软件配置

### config.h 已更新的配置

```c
// I2S引脚配置
#define I2S_BCLK_PIN      GPIO_NUM_15    // SCLK ✅
#define I2S_LRCK_PIN      GPIO_NUM_16    // LRCK ✅
#define I2S_DIN_PIN       GPIO_NUM_17    // DIN (播放) ✅
#define I2S_DOUT_PIN      GPIO_NUM_18    // DOUT (录音) ✅
#define I2S_MCLK_PIN      GPIO_NUM_38    // MCLK ✅

// I2C引脚配置
#define I2C_SCL_PIN       GPIO_NUM_8     // SCL ✅
#define I2C_SDA_PIN       GPIO_NUM_9     // SDA ✅

// 注意: 路小班模块没有SD_MODE和PA_EN引脚
// 这些定义保留仅为避免编译错误,实际不使用
#define SD_MODE_PIN       GPIO_NUM_10    // ❌ 模块无此引脚
#define PA_EN_PIN         GPIO_NUM_11    // ❌ 模块无此引脚
```

### I2S和I2C参数 (无需修改)

```c
// I2S配置
#define I2S_SAMPLE_RATE   16000          // 录音 16kHz
#define I2S_SAMPLE_RATE_TTS 16000        // 播放 16kHz (可改为24000)
#define I2S_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define I2S_CHANNEL_NUM   1              // 单声道
#define I2S_DMA_BUF_COUNT 8
#define I2S_DMA_BUF_LEN   512

// I2C配置
#define I2C_FREQ_HZ       100000         // 100kHz
#define ES8311_I2C_ADDR   0x18           // 或0x19(ADDPIN=VDD)
```

---

## 📋 接线步骤

### 第一步: 电源连接 (⚠️ 重要: 使用5V!)

```
模块 5V → ESP32-S3 5V引脚 或 外部5V电源
模块 GND → ESP32-S3 GND
```

**⚠️⚠️⚠️ 注意:**
- 路小班模块需要**5V供电**,不是3.3V!
- 如果ESP32-S3开发板有5V引脚,可以直接连接
- 如果没有,需要外部5V电源(但要共地)

### 第二步: I2S音频接口

按颜色顺序连接 (从低频到高频):

```
1. GND (黑色粗线) ──→ GND
2. 5V  (红色粗线) ──→ 5V
3. SCLK/BCLK (黄色) → GPIO15
4. LRCK (绿色) ────→ GPIO16
5. DIN (蓝色) ─────→ GPIO17  (播放: ESP32→模块)
6. DOUT (紫色) ────→ GPIO18  (录音: 模块→ESP32)
7. MCLK (白色) ────→ GPIO38  (可选,推荐使用)
```

### 第三步: I2C控制接口

```
8. SCL (橙色) ──→ GPIO8
9. SDA (棕色) ──→ GPIO9
```

### 第四步: 扬声器连接

```
模块 SPK+ → 扬声器正极
模块 SPK- → 扬声器负极
```

### 第五步: 耳机 (可选)

```
直接插入3.5mm耳机插座
```

---

## ✅ 测试验证

### 硬件连接检查

```bash
# 1. 上电前检查
- [ ] 5V和GND未接反
- [ ] 所有I2S引脚正确连接
- [ ] I2C引脚正确连接
- [ ] 扬声器连接到SPK+/SPK-

# 2. 上电后测试
- [ ] 模块指示灯亮(如果有)
- [ ] 用万用表测量5V电压正常
- [ ] 无短路发热
```

### 软件测试

```c
// 测试I2C通信
esp_err_t test_i2c(void) {
    ESP_LOGI(TAG, "测试I2C通信...");
    uint8_t chip_id;
    esp_err_t ret = es8311_read_register(codec, 0x00, &chip_id);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ I2C通信正常 - ES8311 ID: 0x%02X", chip_id);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "❌ I2C通信失败 - 检查GPIO8(SCL)和GPIO9(SDA)");
        return ret;
    }
}

// 测试麦克风录音
void test_microphone(void) {
    ESP_LOGI(TAG, "📢 测试麦克风录音 (3秒)...");
    audio_manager_start_recording(mgr);
    vTaskDelay(pdMS_TO_TICKS(3000));
    audio_manager_stop_recording(mgr);
    ESP_LOGI(TAG, "✅ 麦克风测试完成");
}

// 测试扬声器播放
void test_speaker(void) {
    ESP_LOGI(TAG, "🔊 测试扬声器播放...");
    generate_test_tone(1000, 8000);  // 1kHz测试音
    audio_manager_play_tts_audio(mgr, test_tone, 16000);
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "✅ 扬声器测试完成");
}
```

---

## 🚨 常见问题排查

### ❌ 问题1: 模块不工作

**可能原因**:
- 电压错误 - 用了3.3V而不是5V
- 5V和GND接反

**解决方案**:
```bash
# 用万用表测量
模块5V引脚电压: 应为 5.0V ±5%
模块GND引脚电压: 应为 0V
```

### ❌ 问题2: I2C通信失败

**可能原因**:
- GPIO8/GPIO9接线错误
- I2C地址配置错误

**解决方案**:
```c
// 检查I2C地址
#define ES8311_I2C_ADDR   0x18   // 尝试这个
// #define ES8311_I2C_ADDR   0x19 // 或者这个

// 使用I2C扫描器检测
void i2c_scanner() {
    ESP_LOGI(TAG, "扫描I2C设备...");
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (i2c_probe_address(addr)) {
            ESP_LOGI(TAG, "找到I2C设备: 0x%02X", addr);
        }
    }
}
```

### ❌ 问题3: 无声音输出

**可能原因**:
- DIN/DOUT接反
- ES8311未正确初始化
- 扬声器未连接

**解决方案**:
```c
// 检查接线
DIN → GPIO17  (ESP32→模块,播放)
DOUT → GPIO18 (模块→ESP32,录音)

// 检查ES8311初始化
es8311_codec_init(codec);

// 检查扬声器
用万用表测试扬声器: 阻抗应为4Ω或8Ω
```

### ❌ 问题4: 录音无声音

**可能原因**:
- DOUT未连接到GPIO18
- ES8311麦克风未启用

**解决方案**:
```c
// 确保DOUT连接到GPIO18
DOUT → GPIO18

// 启用ES8311麦克风
es8311_set_input_mute(codec, false);
es8311_set_input_gain(codec, 24.0);  // +24dB增益
```

---

## 📖 参考资料

### 项目文档

- **config.h**: `src/config.h` - 已更新为路小班模块配置
- **ES8311驱动**: `src/drivers/es8311_codec_v2.c`
- **音频管理**: `src/middleware/audio_manager.c`

### 外部资源

- [ES8311数据手册](E:\graduatework\files\ES8311音频芯片数据手册.pdf)
- [NS4150B数据手册](E:\graduatework\files\NS4150B音频功放数据手册.pdf)
- [路小班模块说明书](E:\graduatework\files\ES8311+NS4150B音频模块使用说明书V1.1.pdf)
- [ESP32-S3 I2S官方指南](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2s.html)

---

## 🎯 快速参考

### 接线口诀

```
5V供电记心间,九根线来接齐全
I2S五线传音频,BCLK LRCK DIN DOUT MCLK
I2C两线做控制,SCL SDA来配置
录音DOUT从模块,播放DIN到模块
共地切记要接好,声音清晰没烦恼
```

### 引脚对应表

| 功能 | 引脚 | 记忆 |
|------|------|------|
| 位时钟 | GPIO15 | 一五一十 |
| 字时钟 | GPIO16 | 一路顺风 |
| 数据入 | GPIO17 | 一起播放 |
| 数据出 | GPIO18 | 一发录音 |
| 主时钟 | GPIO38 | 3-8可选 |
| I2C时钟 | GPIO8 |  |
| I2C数据 | GPIO9 |  |

---

**文档版本**: v1.0
**创建日期**: 2025-01-25
**适用模块**: 路小班ES8311+NS4150B (9引脚版本)
**目标平台**: ESP32-S3-N16R8
**电源**: ⚠️ **5V (不是3.3V!)**
