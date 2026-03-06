# 科大讯飞语音听写集成指南

## 概述

本文档介绍如何在ESP32小智AI语音助手中集成科大讯飞语音听写服务，替代原有的云端ASR识别。

## 科大讯飞服务优势

- **国内访问速度快**: 服务器在国内，延迟低
- **中文识别准确率高**: 专门针对中文优化
- **支持多种方言**: 支持普通话、粤语、四川话等
- **价格优惠**: 免费额度充足（500次/天）

---

## 快速开始

### 1. 注册科大讯飞开放平台账号

1. 访问 [科大讯飞开放平台](https://www.xfyun.cn/)
2. 注册账号并登录
3. 创建应用，获取以下认证信息：
   - **APPID**: 应用ID
   - **APIKey**: API密钥
   - **APISecret**: API密钥

### 2. 配置认证信息

在 `main_new.c` 或配置文件中添加你的认证信息：

```c
// 科大讯飞ASR配置
#define IFLYTEK_APPID     "9ed12221"
#define IFLYTEK_API_KEY   "b1ffeca6c122160445ebd4a4d69003b4"
#define IFLYTEK_API_SECRET "NmYwODk4ODVlNGE2YWZhNGM2YjhmMjE4"
```

### 3. 启用科大讯飞ASR

在 `main_new.c` 中初始化科大讯飞模块：

```c
#include "modules/iflytek_asr.h"

// ASR事件回调
static void iflytek_asr_event_cb(iflytek_asr_event_t event,
                                   const iflytek_asr_result_t *result,
                                   void *user_data) {
    switch (event) {
        case IFLYTEK_ASR_EVENT_CONNECTED:
            ESP_LOGI(TAG, "科大讯飞ASR已连接");
            break;

        case IFLYTEK_ASR_EVENT_LISTENING_START:
            ESP_LOGI(TAG, "开始听写");
            break;

        case IFLYTEK_ASR_EVENT_RESULT_PARTIAL:
            if (result) {
                ESP_LOGI(TAG, "临时结果: %s", result->text);
            }
            break;

        case IFLYTEK_ASR_EVENT_RESULT_FINAL:
            if (result) {
                ESP_LOGI(TAG, "最终结果: %s (置信度: %d%%)",
                         result->text, result->confidence);
                // 在这里处理识别结果，例如发送给对话模块
            }
            break;

        case IFLYTEK_ASR_EVENT_ERROR:
            ESP_LOGE(TAG, "科大讯飞ASR错误");
            break;

        default:
            break;
    }
}

// 在app_main()中初始化
void app_main(void) {
    // ... 其他初始化代码 ...

    // 初始化科大讯飞ASR
    iflytek_asr_config_t asr_config = {
        .appid = IFLYTEK_APPID,
        .api_key = IFLYTEK_API_KEY,
        .api_secret = IFLYTEK_API_SECRET,
        .language = "zh_cn",        // 中文
        .domain = "iat",          // 通用听写
        .enable_punctuation = true,  // 开启标点
        .enable_nlp = true,          // 开启NLP优化
        .sample_rate = 16000,        // 16kHz
    };

    esp_err_t ret = iflytek_asr_init(&asr_config, iflytek_asr_event_cb, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "科大讯飞ASR初始化失败: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "科大讯飞ASR初始化成功");
    }

    // ... 其他代码 ...
}
```

---

## 工作流程

### 语音听写完整流程

```
1. 用户说话
       ↓
2. 音频管理器采集音频 (16kHz, 16-bit, 单声道)
       ↓
3. 科大讯飞ASR模块发送音频到服务器
       ↓
4. 科大讯飞服务器返回识别结果
       ↓
5. 触发回调函数，处理识别文本
       ↓
6. 发送给对话模块进行回复
```

---

## API参考

### 初始化与反初始化

```c
// 初始化科大讯飞ASR模块
esp_err_t iflytek_asr_init(
    const iflytek_asr_config_t *config,  // 配置参数
    iflytek_asr_event_cb_t event_cb,      // 事件回调
    void *user_data                        // 用户数据
);

// 反初始化
esp_err_t iflytek_asr_deinit(void);
```

### 连接管理

```c
// 连接科大讯飞服务器
esp_err_t iflytek_asr_connect(void);

// 断开连接
esp_err_t iflytek_asr_disconnect(void);

// 检查连接状态
bool iflytek_asr_is_connected(void);
```

### 听写控制

```c
// 开始听写
esp_err_t iflytek_asr_start_listening(void);

// 停止听写
esp_err_t iflytek_asr_stop_listening(void);

// 发送音频数据
esp_err_t iflytek_asr_send_audio(const uint8_t *data, size_t len);

// 检查是否正在听写
bool iflytek_asr_is_listening(void);
```

### 状态获取

```c
// 获取当前状态
iflytek_asr_state_t iflytek_asr_get_state(void);
```

---

## 配置参数

### iflytek_asr_config_t

| 字段 | 类型 | 说明 | 示例 |
|------|------|------|------|
| appid | char[16] | 应用ID | "9ed12221" |
| api_key | char[32] | API密钥 | "b1ffeca6c..." |
| api_secret | char[64] | API密钥 | "NmYwODk4ODV..." |
| language | char[8] | 语言 | "zh_cn", "en_us" |
| domain | char[16] | 应用领域 | "iat" |
| enable_punctuation | bool | 开启标点 | true |
| enable_nlp | bool | 开启NLP优化 | true |
| sample_rate | uint32_t | 采样率 | 16000 |

---

## 故障排除

### 常见问题

**1. 连接失败**

- 检查WiFi连接是否正常
- 检查APPID、APIKey、APISecret是否正确
- 检查系统时间是否正确（鉴权需要时间同步）

**2. 识别率低**

- 检查麦克风是否正常工作
- 降低VAD阈值以便更容易触发
- 检查音频采样率和格式是否正确
- 确认科大讯飞账户还有剩余调用次数

**3. 延迟高**

- 检查网络连接质量
- 适当调整音频发送间隔
- 考虑使用UDP而不是WebSocket（如果支持）

---

## 注意事项

1. **账户限制**：免费账户每天限制500次调用，超出需要付费
2. **音频格式**：必须使用PCM 16-bit, 16kHz, 单声道
3. **网络要求**：需要稳定的互联网连接
4. **隐私保护**：音频数据会上传到科大讯飞服务器，注意隐私保护

---

## 参考链接

- [科大讯飞开放平台](https://www.xfyun.cn/)
- [语音听写API文档](https://www.xfyun.cn/doc/asr/voicedictation/API.html)
- [鉴权说明](https://www.xfyun.cn/doc/asr/voicedictation/Authentication.html)
