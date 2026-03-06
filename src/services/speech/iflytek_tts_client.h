/**
 * @file iflytek_tts.h
 * @brief 科大讯飞在线语音合成TTS模块头文件
 *
 * 使用科大讯飞WebSocket语音合成API:
 * - 支持中文语音合成
 * - HMAC-SHA256鉴权
 * - 实时流式音频输出
 *
 * 参考文档: https://www.xfyun.cn/doc/tts/online_tts/API.html
 */

#ifndef IFLYTEK_TTS_H
#define IFLYTEK_TTS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 配置常量 ====================

/**
 * @brief 科大讯飞TTS API配置
 * 默认优先 tts-api（官方流式TTS端点），必要时回退 ws-api
 */
#define IFLYTEK_TTS_HOST           "tts-api.xfyun.cn"
#define IFLYTEK_TTS_FALLBACK_HOST  IFLYTEK_TTS_HOST
#define IFLYTEK_TTS_SIGNATURE_HOST IFLYTEK_TTS_HOST
#define IFLYTEK_TTS_PORT           443
#define IFLYTEK_TTS_PATH           "/v2/tts"
#define IFLYTEK_TTS_URL            "wss://tts-api.xfyun.cn/v2/tts"

/**
 * @brief 音频参数
 */
#define IFLYTEK_TTS_SAMPLE_RATE    16000     // 采样率 16kHz
#define IFLYTEK_TTS_BITS           16        // 采样位数 16bit
#define IFLYTEK_TTS_CHANNELS       1         // 单声道

/**
 * @brief 网络参数
 */
#define IFLYTEK_TTS_CONNECT_TIMEOUT_MS  10000     // 连接超时时间
#define IFLYTEK_TTS_MAX_TEXT_LEN       2048       // 最大文本长度

// ==================== 类型定义 ====================

/**
 * @brief TTS事件类型
 */
typedef enum {
    IFLYTEK_TTS_EVENT_IDLE = 0,           // 空闲状态
    IFLYTEK_TTS_EVENT_CONNECTING,         // 正在连接
    IFLYTEK_TTS_EVENT_CONNECTED,          // 已连接
    IFLYTEK_TTS_EVENT_DISCONNECTED,       // 已断开
    IFLYTEK_TTS_EVENT_SYNTHESIZING,       // 正在合成
    IFLYTEK_TTS_EVENT_AUDIO_DATA,         // 收到音频数据
    IFLYTEK_TTS_EVENT_COMPLETE,           // 合成完成
    IFLYTEK_TTS_EVENT_ERROR               // 错误
} iflytek_tts_event_t;

/**
 * @brief TTS音频回调数据
 */
typedef struct {
    const uint8_t *audio_data;    // 音频数据 (PCM 16-bit 16kHz)
    size_t audio_len;             // 音频数据长度
    bool is_final;                // 是否为最后一段音频
} iflytek_tts_audio_t;

/**
 * @brief TTS配置结构
 */
typedef struct {
    char appid[32];               // APPID
    char api_key[64];             // APIKey
    char api_secret[128];         // APISecret
    char voice_name[32];          // 发音人 (x4_yezi: 中文女声, x4_lingxiaoxuan_oral: 等)
    uint32_t sample_rate;         // 采样率
    uint8_t speed;                // 语速 (0-100, 默认50)
    uint8_t volume;               // 音量 (0-100, 默认50)
    uint8_t pitch;                // 音高 (0-100, 默认50)
} iflytek_tts_config_t;

/**
 * @brief TTS事件回调函数类型
 * @param event 事件类型
 * @param audio 音频数据（仅IFLYTEK_TTS_EVENT_AUDIO_DATA时有效）
 * @param user_data 用户数据
 */
typedef void (*iflytek_tts_event_cb_t)(
    iflytek_tts_event_t event,
    const iflytek_tts_audio_t *audio,
    void *user_data
);

/**
 * @brief TTS状态
 */
typedef enum {
    IFLYTEK_TTS_STATE_IDLE = 0,       // 空闲
    IFLYTEK_TTS_STATE_CONNECTING,     // 连接中
    IFLYTEK_TTS_STATE_CONNECTED,      // 已连接
    IFLYTEK_TTS_STATE_SYNTHESIZING,   // 合成中
    IFLYTEK_TTS_STATE_ERROR           // 错误
} iflytek_tts_state_t;

// ==================== 函数声明 ====================

/**
 * @brief 初始化科大讯飞TTS模块
 * @param config TTS配置
 * @param event_cb 事件回调函数
 * @param user_data 用户数据
 * @return ESP_OK成功
 */
esp_err_t iflytek_tts_init(
    const iflytek_tts_config_t *config,
    iflytek_tts_event_cb_t event_cb,
    void *user_data
);

/**
 * @brief 反初始化TTS模块
 * @return ESP_OK成功
 */
esp_err_t iflytek_tts_deinit(void);

/**
 * @brief 合成语音
 * @param text 要合成的文本 (UTF-8编码)
 * @return ESP_OK成功
 */
esp_err_t iflytek_tts_synthesize(const char *text);

/**
 * @brief 停止合成
 * @return ESP_OK成功
 */
esp_err_t iflytek_tts_stop(void);

/**
 * @brief 获取当前状态
 * @return 当前状态
 */
iflytek_tts_state_t iflytek_tts_get_state(void);

/**
 * @brief 检查是否正在合成
 * @return true正在合成
 */
bool iflytek_tts_is_synthesizing(void);

/**
 * @brief 检查是否已连接
 * @return true已连接
 */
bool iflytek_tts_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // IFLYTEK_TTS_H
