# 路小班ES8311+NS4150B模块 - 快速接线参考卡

> **模块**: 路小班 ES8311 + NS4150B 音频模块
> **目标**: ESP32-S3-N16R8 开发板

---

## 📌 快速接线表

| 模块引脚 | ESP32-S3引脚 | 颜色 | 说明 |
|---------|------------|------|------|
| **BCLK** | GPIO15 | 🟡 | I2S位时钟 |
| **LRCK** | GPIO16 | 🟢 | I2S左右声道时钟 |
| **DIN** | GPIO17 | 🔵 | I2S数据输入 (ESP32→模块,播放) |
| **DOUT** | GPIO18 | 🟣 | I2S数据输出 (模块→ESP32,录音) |
| **MCLK** | GPIO38 | ⚪ | I2S主时钟 (推荐使用) |
| **SCL** | GPIO8 | 🟠 | I2C时钟 |
| **SDA** | GPIO9 | 🟤 | I2C数据 |
| **SD_MODE** | GPIO10 | 🔴 | 麦克风(HIGH)/耳机(LOW)切换 |
| **PA_EN** | GPIO11 | ⚫ | 功放使能 (HIGH=开) |
| **VDD** | 3.3V | 🔴 | 电源正极 |
| **GND** | GND | ⚫ | 电源地 |

---

## 🔴 不可使用的GPIO (必须避开!)

| GPIO | 问题 | 原配置 | 修改后 |
|------|------|--------|--------|
| GPIO1 | UART0_TX冲突 | ❌ I2S_MCLK | ✅ GPIO38 |
| GPIO2 | USB D- strapping | ❌ LED | ✅ GPIO47 |
| GPIO12 | MTDO strapping | ❌ SD_MODE | ✅ GPIO10 |
| GPIO13 | MTDI strapping | ❌ PA_EN | ✅ GPIO11 |

---

## ⚙️ 代码配置

### src/config.h 修改:

```c
// I2S引脚
#define I2S_BCLK_PIN      GPIO_NUM_15
#define I2S_LRCK_PIN      GPIO_NUM_16
#define I2S_DIN_PIN       GPIO_NUM_17
#define I2S_DOUT_PIN      GPIO_NUM_18
#define I2S_MCLK_PIN      GPIO_NUM_38    // ✅ 改为GPIO38

// I2C引脚
#define I2C_SCL_PIN       GPIO_NUM_8     // ✅ 改为GPIO8
#define I2C_SDA_PIN       GPIO_NUM_9     // ✅ 改为GPIO9

// 控制引脚
#define SD_MODE_PIN       GPIO_NUM_10    // ✅ 改为GPIO10
#define PA_EN_PIN         GPIO_NUM_11    // ✅ 改为GPIO11
#define LED_STATUS_PIN    GPIO_NUM_47    // ✅ 改为GPIO47
```

---

## 📋 接线步骤

### 1. 电源连接 (必须先接!)
```
模块VDD → ESP32-S3 3.3V
模块GND → ESP32-S3 GND
```

### 2. I2S音频接口 (按颜色连接)
```
BCLK (黄色)  → GPIO15
LRCK (绿色)  → GPIO16
DIN  (蓝色)  → GPIO17  (播放)
DOUT (紫色)  → GPIO18  (录音)
MCLK (白色)  → GPIO38  (可选)
```

### 3. I2C控制接口
```
SCL (橙色) → GPIO8
SDA (棕色) → GPIO9
```

### 4. 控制信号
```
SD_MODE (红色) → GPIO10  (HIGH=麦克风)
PA_EN   (黑色) → GPIO11  (HIGH=功放开)
```

### 5. 扬声器连接
```
SPK+ → 扬声器正极
SPK- → 扬声器负极
```

---

## ✅ 测试检查清单

硬件连接:
- [ ] VDD接3.3V, GND接GND
- [ ] 所有I2S引脚正确连接
- [ ] I2C引脚正确连接
- [ ] 扬声器连接到SPK+/SPK-

代码配置:
- [ ] I2S_MCLK改用GPIO38
- [ ] I2C改用GPIO8/9
- [ ] SD_MODE改用GPIO10
- [ ] PA_EN改用GPIO11
- [ ] LED改用GPIO47

功能测试:
- [ ] I2C通信成功
- [ ] 麦克风录音正常
- [ ] 扬声器播放正常
- [ ] 功放开关控制正常

---

## 🚨 常见错误

### ❌ 错误1: DIN/DOUT接反
```
错误: DIN → GPIO18, DOUT → GPIO17
正确: DIN → GPIO17, DOUT → GPIO18
```

### ❌ 错误2: I2C接反
```
错误: SCL → GPIO9, SDA → GPIO8
正确: SCL → GPIO8, SDA → GPIO9
```

### ❌ 错误3: 使用了strapping pins
```
错误: SD_MODE=GPIO12, PA_EN=GPIO13
正确: SD_MODE=GPIO10, PA_EN=GPIO11
```

---

## 📖 相关文档

- **完整适配指南**: `docs/LUXIAOBAN_MODULE_ADAPTER.md`
- **通用接线指南**: `docs/GPIO_WIRING_GUIDE.md`
- **引脚对比表**: `docs/GPIO_PIN_COMPARISON.md`
- **快速接线图**: `docs/WIRING_DIAGRAM.md`

---

**创建时间**: 2025-01-25
**版本**: v1.0
