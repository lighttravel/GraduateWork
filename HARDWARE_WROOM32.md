# ESP32-WROOM-32 硬件适配说明

## 硬件模块

- **主控**: ESP32-WROOM-32 (ESP32-D0WDQ6)
- **音频模块**: 路小班 ES8311 + NS4105B CODEC DIY小智模块

## 引脚映射表

| 功能 | 引脚定义 | WROOM-32引脚 | 物理位置 | 状态 |
|:-----|:---------|:------------|:---------|:-----|
| **I2S音频接口** |||||
| I2S_SCLK (BCLK) | GPIO15 | Pin 23 | 右侧 | ✅ |
| I2S_LRCK | GPIO16 | Pin 29 | 底部 | ✅ |
| I2S_DIN (SDIN) | GPIO17 | Pin 16 | 左侧 | ✅ |
| I2S_DOUT (SDOUT) | GPIO18 | Pin 21 | 左侧 | ✅ |
| I2S_MCLK | GPIO14 | Pin 12 | 左侧 | ✅ |
| **I2C控制** |||||
| I2C_SCL | GPIO4 | Pin 27 | 底部 | ✅ |
| I2C_SDA | GPIO5 | Pin 22 | 左侧 | ✅ |
| **音频控制** |||||
| SD_MODE | GPIO12 | Pin 12 | 左侧 | ⚠️ 可选 |
| PA_EN | GPIO13 | Pin 11 | 左侧 | ⚠️ 可选 |
| **其他** |||||
| LED_STATUS | GPIO2 | Pin 24 | 底部 | ✅ |

## 供电连接

```
外部5V电源(≥1A, 推荐2A)
    │
    ├────────> ESP32-WROOM-32 VIN (Pin 36)
    │
    └────────> 路小班模块 5V

共地
    │
    ├────────> ESP32-WROOM-32 GND (Pin 13/26/36)
    │
    └────────> 路小班模块 GND
```

**注意**：
- ⚠️ 不要使用ESP32-WROOM-32的3V3引脚给路小班模块供电
- ⚠️ 音频功放需要较大电流，建议使用独立的5V/2A电源
- ⚠️ 如果使用USB供电，可能功率不足，导致音频失真或重启

## 路小班模块接口连接

### 必须连接的接口

```
路小班模块    →    ESP32-WROOM-32
────────────────────────────────
SDA           →    GPIO5 (Pin 22)
SCL           →    GPIO4 (Pin 27)
MCLK          →    GPIO14(Pin 12)
SCLK          →    GPIO15(Pin 23)
DOUT          →    GPIO18(Pin 21)
LRCK          →    GPIO16(Pin 29)
DIN           →    GPIO17(Pin 16)
5V            →    VIN (Pin 36) [通过外部5V电源]
GND           →    GND (Pin 13/26/36)
```

### 可选连接的接口

```
路小班模块    →    ESP32-WROOM-32    用途
─────────────────────────────────────
SD_MODE       →    GPIO12(Pin 12)     麦克风/耳机切换
PA_EN        →    GPIO13(Pin 11)     功放使能控制
```

**注意**：由于路小班模块上MIC和SPK已内置，这两个引脚可能不需要连接。

### 已内置的功能

```
MIC  - 麦克风输入 (已在模块上接好)
SPK  - 扬声器输出 (已在模块上接好)
```

## ESP32-WROOM-32 引脚图

```
        ESP32-WROOM-32 模块俯视图
┌─────────────────────────────────┐
│ EN  1│2 GND 3V3│4 GPIO34│  ← 天线侧
│ GPIO32 5│6 GPIO33 GPIO35│7 GPIO39│
│ GPIO36 8│9 GPIO37 GPIO38│10 GPIO41│
│ GPIO13 11│12 GPIO12 GPIO14│13 GND  │
│ GPIO27 14│15 GPIO26 GPIO25│16 GPIO17 │
│ GPIO23 17│18 GPIO22 GPIO21│19 GPIO3  │
│ GPIO1 20│21 GPIO19 GPIO18│22 GPIO5  │
│ GPIO15 23│24 GPIO2 GND │25 TX0  │  ← USB口侧
│ RX0 26│27 GPIO4 GND │28 RX2  │
│ GPIO16 29│30 GPIO0 GPIO9 │31 GPIO10│
│ GPIO11 32│33 GPIO6 GPIO7 │34 GPIO8  │
│ GPIO24 35│36 GND VIN │37 RST  │
└─────────────────────────────────┘
  35mm间距    35mm间距
```

## 特殊说明

### 1. I2C上拉电阻

路小班模块应该在板载了I2C上拉电阻（通常是4.7kΩ到3.3V）。

如果I2C通信失败，检查：
- 万用表测量SDA和SCL在3.3V上的上拉电阻
- 如果没有，需要在ESP32-WROOM-32的GPIO4/5到3V3之间各加一个4.7kΩ电阻

### 2. MCLK可选性

I2S主时钟MCLK (GPIO14)在某些配置下是可选的。
- 如果路小班模块使用内部时钟，可以不连接MCLK
- 如果发现音频不稳定或无声，尝试连接MCLK

### 3. GPIO12/13的用途

当前代码中：
- `gpio_driver_set_audio_input(true)` 会设置 SD_MODE (GPIO12) 为HIGH
- `gpio_driver_enable_pa(true)` 会设置 PA_EN (GPIO13) 为HIGH

**路小班模块可能已内部处理这些控制**，所以：
1. 先不连接GPIO12/13测试
2. 如果ES8311检测失败或音频无输出，再连接这两根线

### 4. LED指示灯 (GPIO2)

可选：在GPIO2 (Pin 24)上连接一个LED（串联220Ω电阻到GND），用于状态指示。

当前代码中GPIO2会在主循环中闪烁，表示系统正在运行。

## 测试步骤

### 1. 硬件连接

按照"必须连接的接口"表格连接所有线缆。

### 2. 上电测试

1. 连接USB到电脑（COM9）
2. 连接外部5V电源到VIN和路小班模块5V
3. 观察路小班模块上的指示灯（如果有）

### 3. 固件烧录

```bash
pio run --target upload --upload-port COM9
```

### 4. 串口监控

```bash
pio device monitor --port COM9 --baud 115200
```

### 5. 预期输出

**成功情况**：
```
I (xxx) I2C_DRV: I2C初始化完成
I (xxx) ES8311: 检测到ES8311设备, 芯片ID: 0xXX
I (xxx) ES8311: ES8311初始化完成
I (xxx) WIFI_MGR: WiFi连接成功
I (xxx) MAIN_MINIMAL: IP地址: 192.168.5.150
I (xxx) WAKENET_MODULE: 开始监听唤醒词
```

**失败情况**：
```
E (xxx) I2C_DRV: I2C读取失败 @0xFD: ESP_FAIL
E (xxx) ES8311: 未检测到ES8311设备
```

### 6. 故障排除

如果I2C通信失败：

1. **检查供电**
   - 用万用表测量路小班模块5V和GND之间是否有5V
   - 测量电流是否正常（通常50-200mA）

2. **检查I2C连接**
   - SDA (GPIO5) 和 SCL (GPIO4) 是否接对
   - 使用万用表通断测试每根线

3. **检查上拉电阻**
   - 测量SDA和SCL在3.3V上的电阻
   - 应该是4.7kΩ左右

4. **尝试连接GPIO12/13**
   - 虽然可能不需要，但有些模块需要这些控制信号

5. **使用I2C扫描工具**
   - 我可以提供一个I2C扫描程序来诊断问题

## 项目配置文件

当前项目配置 **完全兼容ESP32-WROOM-32**，无需修改。

`src/config.h` 中的所有引脚定义都可以在ESP32-WROOM-32上找到对应引脚。

## 参考资料

- ESP32-WROOM-32数据手册: https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf
- 路小班ES8311模块: 商品页面的使用说明
- 项目README.md: 完整项目文档
