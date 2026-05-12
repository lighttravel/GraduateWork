一、https://github.com/lighttravel/GraduateWork
上述是我在散装版pcb里的代码，目前我画了个板子做了新的pcb需要你来迁移补充完善高效化代码，做出可编译固件
doc\image.png 是pcb板图，注意连线
二、图中电路主要连线可总结如下：
1. ESP32 与 4G 模块 ML307R连接：
4G_TX 接 ESP32 的 P9
4G_RX 接 ESP32 的 P10
4G 模块 ML307R
ML307R 由 5V 供电，并通过 UART 与 ESP32 通信。
2. 音频模块
音频模块 U2 使用 I2S 与 ESP32 连接，另有 ES8311 控制线。
ES8311_SCL 接 ESP32 P4
ES8311_SDA 接 ESP32 P5
I2S_MCLK 接 ESP32 P6
I2S_DIN 接 ESP32 P11
I2S_LRCK 接 ESP32 P12
I2S_DOUT 接 ESP32 P13
I2S_BCLK 接 ESP32 P14
#3. 圆屏显示接口
目前不需要配置该圆屏
P1-8 BL 接 TFT_BL
SPI / 控制信号
P1-3 SCL 接 TFT_SCLK
P1-4 SDA 接 TFT_MOSI
P1-5 RES 接 TFT_DC
P1-6 DC 接 TFT_CS
P1-7 CS 接 TFT_RST

对应 ESP32 侧：

TFT_RST 接 ESP32 P7
TFT_SCLK 接 ESP32 P15
TFT_MOSI 接 ESP32 P16
TFT_DC 接 ESP32 P17
TFT_CS 接 ESP32 P18
TFT_BL 接 ESP32 P47

注意：图中 P1 的 RES/DC/CS 与网络名的对应关系看起来有些异常，例如 P1 的 RES 接到了 TFT_DC，DC 接到了 TFT_CS，CS 接到了 TFT_RST。建议再核对显示屏接口定义，防止丝印或连线顺序错误。
4. 按键电路
有两个按键：
KEY1 接 ESP32 P8
KEY2 接 ESP32 P3
按键按下时，对应 GPIO 被拉低。
