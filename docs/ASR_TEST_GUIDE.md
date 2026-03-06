# 科大讯飞ASR功能测试指南

## 概述

本指南介绍如何测试ESP32小智AI语音助手中的科大讯飞语音听写功能。

## 测试准备

### 1. 硬件要求
- ESP32开发板
- ES8311音频编解码器模块
- 麦克风
- 扬声器/耳机
- 稳定的WiFi连接

### 2. 软件配置
- 确认`config.h`中的WiFi配置正确
- 确认科大讯飞认证信息已正确配置

## 测试步骤

### 第一步：上电初始化

1. 连接ESP32到电脑
2. 打开串口监视器：
   ```bash
   pio device monitor --baud 115200
   ```
3. 给ESP32上电
4. 观察串口输出，应该看到：
   ```
   [MAIN] 小智AI语音助手启动
   [MAIN] 版本: v2.0 - 小智AI方案
   [WIFI_MGR] WiFi已连接
   [AUDIO_MGR] 音频管理器初始化完成
   [IFLYTEK_ASR] 科大讯飞ASR模块初始化成功
   [IFLYTEK_ASR] APPID: 9ed12221
   ```

### 第二步：测试唤醒功能

1. 等待系统进入空闲模式，看到日志：
   ```
   [MAIN] 进入空闲模式，等待唤醒词...
   [WAKENET] 进入空闲模式，等待唤醒词...
   ```

2. 对着麦克风说出唤醒词"小智小智"

3. 观察串口输出，应该看到：
   ```
   [WAKENET] 检测到唤醒词！置信度: 0.85
   [MAIN] 连接到服务器...
   [IFLYTEK_ASR] 科大讯飞ASR HTTP客户端已初始化
   [IFLYTEK_ASR] 科大讯飞ASR开始听写
   ```

### 第三步：测试语音识别

1. 唤醒成功后，系统进入听写模式，看到日志：
   ```
   [MAIN] 开始监听用户语音...
   [IFLYTEK_ASR] 开始听写，请说话...
   ```

2. 对着麦克风说出中文，例如：
   - "今天天气怎么样"
   - "打开灯"
   - "播放音乐"

3. 观察串口输出，应该看到：
   ```
   [IFLYTEK_ASR] 临时结果: 今天
   [IFLYTEK_ASR] 临时结果: 今天天气
   [IFLYTEK_ASR] 临时结果: 今天天气怎么
   [IFLYTEK_ASR] 最终结果: 今天天气怎么样 (置信度: 95%)
   ```

### 第四步：测试对话流程

1. 识别成功后，系统会自动调用对话模块，看到日志：
   ```
   [MAIN] 识别到文本: 今天天气怎么样
   [CHAT] 发送文本到对话模块...
   ```

2. 等待服务器返回TTS音频，看到日志：
   ```
   [TTS] 开始接收TTS音频
   [AUDIO_MGR] 开始播放
   ```

3. 播放完成后，系统自动回到监听状态：
   ```
   [TTS] TTS播放完成，继续监听...
   [MAIN] TTS播放完成，继续监听...
   ```

## 故障排除

### 问题1：唤醒词检测不灵敏

**症状**: 喊出"小智小智"没有反应

**解决方案**:
1. 检查麦克风是否连接正确
2. 调整VAD阈值：
   ```c
   // 在config.h中降低阈值
   #define VAD_THRESHOLD 100  // 原来是500
   ```
3. 确保ES8311的MicBias已正确配置
4. 检查串口日志中是否有I2S数据

### 问题2：无法连接到科大讯飞服务器

**症状**: 日志显示"连接服务器失败"

**解决方案**:
1. 检查WiFi连接是否正常
2. 验证认证信息：
   ```c
   // 检查这些值是否正确
   #define IFLYTEK_APPID     "9ed12221"
   #define IFLYTEK_API_KEY   "b1ffeca6c122160445ebd4a4d69003b4"
   #define IFLYTEK_API_SECRET "NmYwODk4ODVlNGE2YWZhNGM2YjhmMjE4"
   ```
3. 检查系统时间是否正确（鉴权需要时间同步）

### 问题3：语音识别率低

**症状**: 识别结果不准确或为空

**解决方案**:
1. 确保麦克风质量良好
2. 降低环境噪音
3. 说话时距离麦克风10-20厘米
4. 检查音频数据是否正常：
   ```c
   // 在i2s_driver.c中启用调试
   #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
   ```

### 问题4：TTS播放没有声音

**症状**: 识别成功但没有听到回复

**解决方案**:
1. 检查扬声器/耳机连接
2. 确认ES8311初始化成功
3. 检查音量设置：
   ```c
   // 在config.h中调整音量
   #define AUDIO_VOLUME 80  // 范围0-100
   ```
4. 检查PA_EN引脚是否正确配置

## 高级调试

### 启用详细日志

在`config.h`中添加：
```c
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
```

### 检查I2S数据

在串口命令行输入：
```
i2s_debug
```
（需要在代码中实现此命令）

### 手动测试ASR

在代码中添加测试函数：
```c
void test_iflytek_asr(void) {
    // 测试连接
    iflytek_asr_connect();

    // 开始听写
    iflytek_asr_start_listening();

    // 等待5秒
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 停止听写
    iflytek_asr_stop_listening();
}
```

## 总结

通过以上测试步骤，您可以全面验证科大讯飞ASR功能的正确性。如果遇到问题，请参考故障排除章节或查看串口日志进行诊断。

如有其他问题，请查阅`docs/IFLYTEK_INTEGRATION_GUIDE.md`获取更多信息。
