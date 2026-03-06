/**
 * @file iflytek_asr.h
 * @brief 科大讯飞语音听写ASR模块头文件
 *
 * 使用科大讯飞WebSocket语音听写API:
 * - 支持实时语音转文字
 * - HMAC-SHA256鉴权
 * - 支持多种音频格式和采样率
 *
 * 参考文档: https://www.xfyun.cn/doc/asr/voicedictation/API.html
 */

#ifndef IFLYTEK_ASR_H
#define IFLYTEK_ASR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 配置常量 ====================

/**
 * @brief 科大讯飞API配置
 * 注意：官方推荐使用iat-api.xfyun.cn（中英文）
 * 参考文档: https://www.xfyun.cn/doc/asr/voicedictation/API.html
 */
#define IFLYTEK_HOST            "iat-api.xfyun.cn"
#define IFLYTEK_PORT            443
#define IFLYTEK_PATH            "/v2/iat"
#define IFLYTEK_URL             "wss://iat-api.xfyun.cn/v2/iat"

/**
 * @brief 音频参数
 */
#define IFLYTEK_AUDIO_SAMPLE_RATE   16000     // 采样率 16kHz
#define IFLYTEK_AUDIO_BITS          16        // 采样位数 16bit
#define IFLYTEK_AUDIO_CHANNELS      1         // 单声道
#define IFLYTEK_AUDIO_FORMAT        "audio/L16;rate=16000"  // 音频格式

/**
 * @brief 网络参数
 */
#define IFLYTEK_FRAME_SIZE          1280      // 每帧音频数据大小 (40ms @ 16kHz 16bit)
#define IFLYTEK_SEND_INTERVAL_MS    40        // 发送间隔
#define IFLYTEK_CONNECT_TIMEOUT_MS  10000     // 连接超时时间
#define IFLYTEK_RECONNECT_INTERVAL_MS 5000    // 重连间隔

// ==================== 类型定义 ====================

/**
 * @brief ASR事件类型
 */
typedef enum {
    IFLYTEK_ASR_EVENT_IDLE = 0,           // 空闲状态
    IFLYTEK_ASR_EVENT_CONNECTING,        // 正在连接
    IFLYTEK_ASR_EVENT_CONNECTED,         // 已连接
    IFLYTEK_ASR_EVENT_DISCONNECTED,      // 已断开
    IFLYTEK_ASR_EVENT_LISTENING_START,   // 开始听写
    IFLYTEK_ASR_EVENT_LISTENING_STOP,    // 停止听写
    IFLYTEK_ASR_EVENT_RESULT_PARTIAL,    // 中间结果
    IFLYTEK_ASR_EVENT_RESULT_FINAL,      // 最终结果
    IFLYTEK_ASR_EVENT_ERROR              // 错误
} iflytek_asr_event_t;

/**
 * @brief ASR结果结构
 */
typedef struct {
    char text[512];           // 识别文本 (增大缓冲区以支持长句)
    bool is_final;            // 是否为最终结果
    int confidence;           // 置信度 (0-100)
    int32_t sentence_id;      // 句子ID
    int32_t begin_time;       // 开始时间 (ms)
    int32_t end_time;         // 结束时间 (ms)
} iflytek_asr_result_t;

/**
 * @brief ASR配置结构
 */
typedef struct {
    char appid[32];           // APPID (增加缓冲区大小)
    char api_key[64];         // APIKey (增加缓冲区大小以确保null终止)
    char api_secret[128];     // APISecret (增加缓冲区大小以确保null终止)
    char language[8];         // 语言 (zh_cn: 中文, en_us: 英文)
    char domain[16];          // 应用领域 (iat: 通用听写, medical: 医疗...)
    bool enable_punctuation;    // 是否开启标点
    bool enable_nlp;            // 是否开启NLP优化
    uint32_t sample_rate;     // 采样率
} iflytek_asr_config_t;

/**
 * @brief ASR事件回调函数类型
 */
typedef void (*iflytek_asr_event_cb_t)(
    iflytek_asr_event_t event,
    const iflytek_asr_result_t *result,
    void *user_data
);

/**
 * @brief ASR状态
 */
typedef enum {
    IFLYTEK_ASR_STATE_IDLE = 0,       // 空闲
    IFLYTEK_ASR_STATE_CONNECTING,     // 连接中
    IFLYTEK_ASR_STATE_CONNECTED,      // 已连接
    IFLYTEK_ASR_STATE_LISTENING,      // 听写中
    IFLYTEK_ASR_STATE_ERROR           // 错误
} iflytek_asr_state_t;

// ==================== 函数声明 ====================

/**
 * @brief 初始化科大讯飞ASR模块
 * @param config ASR配置
 * @param event_cb 事件回调函数
 * @param user_data 用户数据
 * @return ESP_OK成功
 */
esp_err_t iflytek_asr_init(
    const iflytek_asr_config_t *config,
    iflytek_asr_event_cb_t event_cb,
    void *user_data
);

/**
 * @brief 反初始化ASR模块
 * @return ESP_OK成功
 */
esp_err_t iflytek_asr_deinit(void);

/**
 * @brief 连接科大讯飞服务器
 * @return ESP_OK成功
 */
esp_err_t iflytek_asr_connect(void);

/**
 * @brief 断开连接
 * @return ESP_OK成功
 */
esp_err_t iflytek_asr_disconnect(void);

/**
 * @brief 开始听写
 * @return ESP_OK成功
 */
esp_err_t iflytek_asr_start_listening(void);

/**
 * @brief 停止听写
 * @return ESP_OK成功
 */
esp_err_t iflytek_asr_stop_listening(void);

/**
 * @brief 发送音频数据
 * @param data 音频数据缓冲区 (PCM 16-bit)
 * @param len 数据长度 (字节)
 * @return ESP_OK成功
 */
esp_err_t iflytek_asr_send_audio(const uint8_t *data, size_t len);

/**
 * @brief 获取当前状态
 * @return 当前状态
 */
iflytek_asr_state_t iflytek_asr_get_state(void);

/**
 * @brief 检查是否已连接
 * @return true已连接
 */
bool iflytek_asr_is_connected(void);

/**
 * @brief 检查是否正在听写
 * @return true正在听写
 */
bool iflytek_asr_is_listening(void);

/**
 * @brief 生成鉴权URL
 * @note 内部使用，生成带签名的WebSocket URL
 * @param url 输出缓冲区
 * @param url_len 缓冲区大小
 * @return ESP_OK成功
 */
esp_err_t iflytek_asr_generate_auth_url(char *url, size_t url_len);

#ifdef __cplusplus
}
#endif

#endif // IFLYTEK_ASR_H
