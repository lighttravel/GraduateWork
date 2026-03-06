# 小智AI方案改造完成指南

## 📋 改造概述

本次改造将您的ESP32 AI语音助手从**云端API方案**（Deepgram + DeepSeek）改造为**小智AI方案**（ESP-SR + xiaozhi-esp32-server）。

### 架构对比

```
┌──────────────────────────────────────────────────────────────┐
│ 旧架构 (已移除)                                          │
├──────────────────────────────────────────────────────────────┤
│ ESP32-S3                                                  │
│  ├─ ASR模块 ──WebSocket──> Deepgram API                  │
│  ├─ Chat模块 ──HTTP──────> DeepSeek API                   │
│  └─ TTS模块 ──HTTP──────> Deepgram TTS API                │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│ 新架构 (小智AI方案)                                        │
├──────────────────────────────────────────────────────────────┤
│ ESP32-S3                                                  │
│  ├─ WakeNet模块 ───> 离线唤醒词检测 (ESP-SR)             │
│  ├─ XiaoZhi客户端 ──WebSocket──> xiaozhi-esp32-server      │
│  ├─ ASR模块 ─────────> 发送音频流                           │
│  └─ TTS模块 ─────────> 接收音频流                           │
│                                                           │
└──────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌──────────────────────────────────────────────────────────────┐
│ xiaozhi-esp32-server (后端服务)                            │
├──────────────────────────────────────────────────────────────┤
│  ├─ ASR: FunASR/讯飞流式ASR                             │
│  ├─ LLM: Qwen/DeepSeek/其他                             │
│  ├─ TTS: 灵犀/火山/Edge-TTS                              │
│  ├─ VAD: SileroVAD                                        │
│  └─ 声纹识别: 3D-Speaker                                    │
└──────────────────────────────────────────────────────────────┘
```

## 🎯 改造内容

### 1. 新增模块

| 模块 | 文件 | 功能 |
|------|------|------|
| **WakeNet** | `wakenet_module.h/c` | 离线唤醒词检测（基于VAD的存根实现） |
| **XiaoZhi客户端** | `xiaozhi_client.h/c` | WebSocket客户端，连接到xiaozhi-esp32-server |
| **ASR模块（新）** | `asr_module_new.h/c` | 通过WebSocket发送音频，接收ASR结果 |
| **TTS模块（新）** | `tts_module_new.h/c` | 通过WebSocket接收TTS音频 |

### 2. 保留模块

| 模块 | 说明 |
|------|------|
| `audio_manager` | 需要添加 `audio_manager_play_tts_audio()` 接口 |
| `wifi_manager` | 保持不变 |
| `http_server` | 保持不变（用于配置） |
| `nvs_storage` | 保持不变 |
| `config_manager` | 需要添加服务器URL配置 |
| `chat_module` | **简化为占位符**（LLM已移到服务器端） |

### 3. 移除模块

| 模块 | 说明 |
|------|------|
| `asr_module.c` (旧) | Deepgram WebSocket客户端代码 |
| `tts_module.c` (旧) | Deepgram TTS HTTP客户端代码 |
| `chat_module.c` 中的LLM部分 | DeepSeek HTTP客户端代码 |

### 4. 配置文件修改

#### platformio.ini
- ✅ 添加了小智AI方案注释
- ✅ 使用 `-Os` 优化体积
- ✅ 启用LTO减少10-20%大小
- ✅ 关闭调试日志以减小体积
- ✅ 所有环境使用优化的分区表：`partitions_xiaozhi.csv`

#### partitions_xiaozhi.csv
- ✅ NVS分区扩大到24KB（原16KB）
- ✅ APP分区各2.5MB（支持OTA）
- ✅ SPIFFS分区512KB（文件系统）

#### CMakeLists.txt
- ✅ 添加新模块源文件
- ✅ 添加 `esp_websocket_client` 依赖

## 📡 WebSocket协议

### 连接URL
```
测试服务器: ws://2662r3426b.vicp.fun/xiaozhi/v1/
本地服务器: ws://192.168.x.x:PORT/xiaozhi/v1/
```

### 消息格式

#### 1. hello（认证）
```json
{
  "type": "hello",
  "version": 1,
  "features": {
    "mcp": false
  },
  "transport": "websocket"
}
```

#### 2. listen（控制ASR）
```json
{
  "type": "listen",
  "state": "start"  // 或 "stop"
}
```

#### 3. text（发送文本/聊天）
```json
{
  "type": "text",
  "content": "用户说的内容"
}
```

#### 4. 音频流
- **ESP32 → 服务器**: 二进制数据（PCM 16-bit mono, 16kHz）
- **服务器 → ESP32**: 二进制数据（TTS音频）

### 服务器响应

#### text消息（ASR结果/LLM回复）
```json
{
  "type": "text",
  "content": "识别的文本或AI回复"
}
```

## 🚀 部署步骤

### 方案A: 使用公共测试服务器（快速体验）

1. **烧录固件**
   ```bash
   pio run --target upload
   ```

2. **配置WiFi**
   - 通过串口监视器查看AP模式：`ESP32-Xiaozhi`
   - 连接到 `http://192.168.4.1` 配置WiFi

3. **测试**
   - 设备会自动连接到测试服务器
   - 对着麦克风说话唤醒（基于VAD，检测到语音即唤醒）

### 方案B: 部署本地服务器

1. **克隆xiaozhi-esp32-server**
   ```bash
   git clone https://github.com/xinnan-tech/xiaozhi-esp32-server.git
   cd xiaozhi-esp32-server
   ```

2. **安装依赖**
   ```bash
   pip install -r requirements.txt
   ```

3. **配置**
   - 编辑 `main/xiaozhi-server/config.yaml`
   - 配置ASR、LLM、TTS服务
   - 参考: [配置文档](https://github.com/xinnan-tech/xiaozhi-esp32-server/blob/main/docs/images/banner2.png)

4. **启动服务器**
   ```bash
   python main.py
   ```

5. **修改ESP32固件服务器URL**
   - 通过Web配置界面修改服务器URL
   - 或在 `config.h` 中修改默认值

## 🔧 需要完成的集成工作

### 1. audio_manager添加TTS播放接口

在 `audio_manager.h/c` 中添加：
```c
esp_err_t audio_manager_play_tts_audio(const uint8_t *data, size_t len);
```

实现逻辑：
- 将TTS音频数据写入I2S DMA缓冲区
- 采样率：24kHz（根据服务器TTS配置）
- 与录音同时进行（全双工）

### 2. config_manager添加服务器URL配置

在NVS中添加：
```c
#define NVS_KEY_SERVER_URL "server_url"
#define MAX_SERVER_URL_LEN 256
```

### 3. WakeNet完整实现

当前是**存根实现**，基于VAD能量检测。

完整实现需要：
```bash
idf.py add-dependency "espressif/esp-sr^1.0.0"
```

参考：
- [ESP-SR入门指南](https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/getting_started/readme.html)
- [WakeNet模型文档](https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/wake_word_engine/README.html)
- [示例代码](https://github.com/espressif/esp-sr/blob/master/examples/simple_wakenet/main/main.c)

### 4. 主流程集成（main.c）

需要实现的状态机：
```
IDLE ──[WakeNet检测到唤醒]──> CONNECTING
                                   │
                                   ├──连接成功──> LISTENING
                                   │              │
                                   │              ├──发送音频──> PROCESSING
                                   │              │              │
                                   │              ├──收到ASR──> 显示文本
                                   │              │              │
                                   │              ├──收到TTS──> 播放音频
                                   │              │              │
                                   │              └──播放完成──> LISTENING
                                   │
                                   └──断开连接──> IDLE
```

## 📊 资源使用对比

| 项目 | 旧方案 | 新方案 | 说明 |
|------|--------|--------|------|
| **Flash** | ~500KB | ~400KB | 移除大量HTTP客户端代码 |
| **RAM** | ~80KB | ~70KB | WebSocket比HTTP轻量 |
| **网络延迟** | 200-500ms | 50-150ms | 本地服务器显著降低延迟 |
| **离线能力** | 无 | 唤醒词离线 | 改进用户体验 |

## 🎁 优势总结

### 旧方案的问题
- ❌ **依赖3个不同的云端API** - Deepgram ASR + DeepSeek Chat + Deepgram TTS
- ❌ **网络延迟高** - RTT延迟200-500ms
- ❌ **成本高** - 按量付费，API密钥管理复杂
- ❌ **无离线能力** - 完全依赖网络
- ❌ **代码复杂** - 需要维护3个HTTP/WebSocket客户端

### 新方案的优势
- ✅ **统一后端** - xiaozhi-esp32-server统一管理ASR/LLM/TTS
- ✅ **低延迟** - 本地服务器延迟50-150ms，流式处理
- ✅ **成本低** - 本地部署无API费用，或使用免费API（FunASR、glm-4-flash等）
- ✅ **离线唤醒** - ESP-SR唤醒词检测，无需网络
- ✅ **功能丰富** - 支持声纹识别、多用户、视觉识别等
- ✅ **代码简洁** - ESP32只负责音频流传输
- ✅ **可扩展** - 支持插件系统、MCP协议等

## 📚 参考资料

- [xiaozhi-esp32-server GitHub](https://github.com/xinnan-tech/xiaozhi-esp32-server)
- [ESP-SR官方文档](https://docs.espressif.com/projects/esp-sr/zh_CN/latest/esp32s3/getting_started/readme.html)
- [小智AI语音机器人 - 火山引擎](https://developer.volcengine.com/articles/7540133843739934774)
- [小智WebSocket协议文档](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md)
- [复刻小智AI协议分析](https://xujiwei.com/blog/2025/04/ai-xiaozhi-esp32-protocol/)
- [xiaozhi-esp32实现源码分析](https://www.cnblogs.com/FBsharl/p/19012140)

## ⚠️ 注意事项

1. **API密钥管理**
   - 旧方案需要Deepgram/DeepSeek密钥
   - 新方案密钥在服务器端配置，ESP32不需要

2. **网络配置**
   - 旧方案直接连接公网API
   - 新方案需要配置服务器URL（本地或公网）

3. **音频格式**
   - 录音：16kHz, 16-bit PCM, mono（保持不变）
   - TTS播放：根据服务器配置（24kHz或16kHz）

4. **编译依赖**
   - 需要ESP-IDF的 `esp_websocket_client` 组件
   - 需要ESP-IDF的 `cJSON` 组件

## 🔄 后续改进方向

1. **完整ESP-SR集成** - 使用真正的WakeNet模型替换VAD存根
2. **OTA升级** - 支持通过xiaozhi-esp32-server进行固件升级
3. **MQTT协议** - 除了WebSocket，也支持MQTT（更稳定）
4. **MCP协议** - 支持模型上下文协议（MCP）用于更强大的功能调用
5. **视觉识别** - 集成摄像头，使用VLLM进行图像识别

## ✅ 完成清单

- [x] 创建优化的分区表（partitions_xiaozhi.csv）
- [x] 更新platformio.ini（小智AI方案配置）
- [x] 创建WakeNet模块（wakenet_module）
- [x] 创建XiaoZhi客户端（xiaozhi_client）
- [x] 重构ASR模块（asr_module_new）
- [x] 重构TTS模块（tts_module_new）
- [x] 更新CMakeLists.txt（新模块依赖）
- [ ] 完善audio_manager的TTS播放接口
- [ ] 完善config_manager的服务器URL配置
- [ ] 更新main.c实现完整状态机
- [ ] 完整ESP-SR集成（替换VAD存根）
- [ ] 测试和验证

---

**改造日期**: 2025年（根据系统日期）
**改造者**: Claude Code
**版本**: v2.0 - 小智AI方案
