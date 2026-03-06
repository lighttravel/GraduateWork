/**
 * @file config.h
 * @brief 硬件引脚配置和系统配置
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 硬件引脚定义 ====================
//
// Moji小智AI衍生版引脚配置 (官方标准配置)
// 基于xiaozhi-esp32项目: https://github.com/78/xiaozhi-esp32
//
// 接线: Moji小智AI/路小班ES8311模块 → ESP32-S3
// ----------------------------------------------------
// I2S音频接口 (双向通信)
// MCLK       → GPIO6   (I2S主时钟,推荐使用)
// SCLK/BCLK  → GPIO14  (I2S位时钟)
// LRCK/WS    → GPIO12  (I2S左右声道时钟/字选择)
// DIN        → GPIO13  (I2S数据输入: ESP32→模块,用于播放)
// DOUT       → GPIO11  (I2S数据输出: 模块→ESP32,用于录音)
//
// I2C控制接口 (ES8311编解码器配置)
// SCL        → GPIO4   (I2C时钟)
// SDA        → GPIO5   (I2C数据)
//
// 电源 (重要: ES8311模块需要5V!)
// 5V         → 5V     (模块外部供电或ESP32 5V引脚)
// GND        → GND    (共地)
// ----------------------------------------------------

// I2S引脚配置 (ESP32-S3 <-> ES8311编解码器)
// 根据实际硬件接线更新
#define I2S_BCLK_PIN      GPIO_NUM_14    // SCLK - I2S串行时钟
#define I2S_LRCK_PIN      GPIO_NUM_12    // LRCK/WS - 左右声道选择
#define I2S_DIN_PIN       GPIO_NUM_11    // DIN - 串行数据输入(到ES8311,用于播放) ✅ 用户实际接线
#define I2S_DOUT_PIN      GPIO_NUM_13    // DOUT - 串行数据输出(从ES8311,用于录音) ✅ 用户实际接线
#define I2S_MCLK_PIN      GPIO_NUM_6     // MCLK - 主时钟

// I2C引脚配置 (ESP32-S3 <-> ES8311编解码器)
#define I2C_SCL_PIN       GPIO_NUM_4     // I2C时钟
#define I2C_SDA_PIN       GPIO_NUM_5     // I2C数据

// 音频模块控制引脚
// 注意: 路小班模块没有SD_MODE和PA_EN引脚,以下定义仅作兼容性保留
#define SD_MODE_PIN       GPIO_NUM_10    // ❌ 路小班模块无此引脚 (保留定义避免编译错误)
#define PA_EN_PIN         GPIO_NUM_11    // ❌ 路小班模块无此引脚 (保留定义避免编译错误)

// LED状态指示引脚
#define LED_STATUS_PIN    GPIO_NUM_47    // 状态LED ✅ 安全 (避开GPIO2 USB D- strapping)

// ==================== I2S配置 ====================

#define I2S_NUM           I2S_NUM_0         // 使用I2S0
#define I2S_SAMPLE_RATE   16000              // 采样率16kHz (ASR)
#define I2S_SAMPLE_RATE_TTS 16000            // 采样率16kHz (TTS播放 - 与讯飞TTS匹配)
#define I2S_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define I2S_CHANNEL_NUM   1                  // 单声道

// DMA缓冲区配置
#define I2S_DMA_BUF_COUNT 8
#define I2S_DMA_BUF_LEN   512               // 每个缓冲区512个采样点

// ==================== I2C配置 ====================

#define I2C_NUM           I2C_NUM_0
#define I2C_FREQ_HZ       100000             // 100kHz (标准I2C频率)
#define ES8311_I2C_ADDR   0x18               // ES8311的I2C地址

// ==================== 音频配置 ====================

#define AUDIO_VOLUME      80                 // 默认音量 (0-100)
// 临时降低 VAD 阈值以便调试（原为500，调低后更容易触发）
#define VAD_THRESHOLD     100                // 语音活动检测阈值（调试模式）
#define MAX_AUDIO_BUF_SIZE (1024 * 16)       // 最大音频缓冲区16KB

// ==================== WiFi配置 ====================

// 默认WiFi配置 (首次启动时使用)
#define DEFAULT_WIFI_SSID      "XUNTIAN_2.4G"
#define DEFAULT_WIFI_PASSWORD  "xuntian13937020766"

#define WIFI_AP_SSID      "ESP32-Xiaozhi"    // AP模式SSID
#define WIFI_AP_PASSWORD  ""                 // AP模式密码(空=开放)
#define WIFI_AP_CHANNEL   1                  // AP信道
#define WIFI_AP_MAX_CONN  4                  // 最大连接数

#define WIFI_CONNECT_TIMEOUT_MS  30000       // WiFi连接超时
#define WIFI_RETRY_COUNT          5          // WiFi重试次数

// ==================== Web服务器配置 ====================

#define WEB_SERVER_PORT   80                 // HTTP端口
#define WS_SERVER_PORT    81                 // WebSocket端口

#define MAX_HTTP_REQ_LEN  2048               // 最大HTTP请求长度
#define MAX_WS_BUF_LEN    4096               // WebSocket缓冲区大小

// ==================== NVS配置 ====================

// NVS命名空间
#define NVS_NAMESPACE     "xiaozhi"

// NVS键名
#define NVS_KEY_WIFI_SSID      "wifi_ssid"
#define NVS_KEY_WIFI_PASS      "wifi_pass"
#define NVS_KEY_DEEPGRAM_KEY   "deepgram_key"
#define NVS_KEY_DEEPSEEK_KEY   "deepseek_key"
#define NVS_KEY_INDEX_TTS_URL  "index_tts_url"
#define NVS_KEY_TTS_PROVIDER   "tts_provider"
#define NVS_KEY_CONFIG_DONE    "config_done"
#define NVS_KEY_DEVICE_NAME    "device_name"
#define NVS_KEY_SERVER_URL     "server_url"        // 小智服务器URL
#define NVS_KEY_IFLYTEK_APPID  "iflytek_appid"     // 科大讯飞APPID
#define NVS_KEY_IFLYTEK_KEY    "iflytek_key"       // 科大讯飞API Key
#define NVS_KEY_IFLYTEK_SECRET "iflytek_secret"    // 科大讯飞API Secret

// 配置值最大长度
#define MAX_SSID_LEN           32
#define MAX_PASSWORD_LEN       64
#define MAX_API_KEY_LEN        128
#define MAX_URL_LEN            256
#define MAX_DEVICE_NAME_LEN    32

// ==================== 小智AI服务器配置 ====================

// 默认小智服务器地址（测试服务器）
#define XIAOZHI_SERVER_URL     "ws://2662r3426b.vicp.fun/xiaozhi/v1/"

// 本地服务器地址（用户自部署时使用）
#define XIAOZHI_SERVER_LOCAL   "ws://192.168.1.100:8000/xiaozhi/v1/"

// 设备ID（可选，用于多用户管理）
#define XIAOZHI_DEVICE_ID       "esp32-001"

// ==================== API配置 ====================

// 科大讯飞ASR配置
#define IFLYTEK_APPID           "9ed12221"
#define IFLYTEK_API_KEY         "b1ffeca6c122160445ebd4a4d69003b4"
#define IFLYTEK_API_SECRET      "NmYwODk4ODVlNGE2YWZhNGM2YjhmMjE4"
#define IFLYTEK_ASR_LANGUAGE    "zh_cn"         // 语言: zh_cn(中文), en_us(英文)
#define IFLYTEK_ASR_DOMAIN      "iat"           // 领域: iat(通用听写)

// Deepgram ASR API
#define DEEPGRAM_ASR_URL       "wss://api.deepgram.com/v1/listen"
#define DEEPGRAM_API_HOST      "api.deepgram.com"
#define DEEPGRAM_API_PORT      443

// DeepSeek Chat API
#define DEEPSEEK_API_URL       "https://api.deepseek.com/v1/chat/completions"
#define DEEPSEEK_API_HOST      "api.deepseek.com"
#define DEEPSEEK_API_PORT      443
#define DEEPSEEK_API_KEY       "sk-e858af6399024030a35798cac18a961a"  // 默认API密钥
#define DEEPSEEK_MODEL         "deepseek-chat"  // DeepSeek模型名称

// Deepgram TTS API
#define DEEPGRAM_TTS_URL       "https://api.deepgram.com/v1/speak"
#define DEEPGRAM_TTS_MODEL     "aura-asteria-en"

// ==================== 对话配置 ====================

#define MAX_CHAT_HISTORY   6                  // 最大对话历史轮数
#define MAX_MESSAGE_LEN    1024               // 单条消息最大长度
#define MAX_RESPONSE_LEN   2048               // AI回复最大长度

// ==================== 系统配置 ====================

#define SYSTEM_TASK_STACK_SIZE  (8 * 1024)    // 系统任务栈大小
#define SYSTEM_TASK_PRIORITY    5             // 系统任务优先级

#define AUDIO_TASK_STACK_SIZE   (12 * 1024)   // 音频任务栈大小
#define AUDIO_TASK_PRIORITY     10            // 音频任务优先级

#define HTTP_TASK_STACK_SIZE    (4 * 1024)    // HTTP任务栈大小
#define HTTP_TASK_PRIORITY      8             // HTTP任务优先级

// ==================== 调试配置 ====================

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG         // 日志级别 (DEBUG以查看ES8311详细日志)

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H
