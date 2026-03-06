/**
 * @file wakenet_module.h
 * @brief 唤醒词检测模块 - 基于ESP-SR的WakeNet
 *
 * 功能：
 * - 使用ESP-SR的WakeNet模型进行离线唤醒词检测
 * - 支持中英文唤醒词
 * - 低功耗监听，检测到唤醒词后触发回调
 */

#ifndef WAKENET_MODULE_H
#define WAKENET_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 类型定义 ====================

/**
 * @brief 唤醒词状态
 */
typedef enum {
    WAKENET_STATE_IDLE = 0,         // 空闲
    WAKENET_STATE_LISTENING,         // 监听中
    WAKENET_STATE_DETECTED,           // 检测到唤醒词
    WAKENET_STATE_ERROR              // 错误
} wakenet_state_t;

/**
 * @brief 唤醒词事件
 */
typedef enum {
    WAKENET_EVENT_LISTENING_START = 0,   // 开始监听
    WAKENET_EVENT_LISTENING_STOP,        // 停止监听
    WAKENET_EVENT_DETECTED,              // 检测到唤醒词
    WAKENET_EVENT_ERROR                 // 错误
} wakenet_event_t;

/**
 * @brief 唤醒词检测结果
 */
typedef struct {
    float score;                // 置信度 (0.0 - 1.0)
    uint32_t timestamp;         // 检测时间戳
} wakenet_result_t;

/**
 * @brief 唤醒词事件回调
 * @param event 唤醒词事件
 * @param result 检测结果(仅WAKENET_EVENT_DETECTED时有值)
 * @param user_data 用户数据
 */
typedef void (*wakenet_event_callback_t)(wakenet_event_t event, const wakenet_result_t *result, void *user_data);

/**
 * @brief 唤醒词配置
 */
typedef struct {
    char wake_word[32];         // 唤醒词名称 (如 "hinet_xiaozhi", "hi_esp")
    int sample_rate;             // 采样率 (默认16000)
    bool vad_enable;             // 是否启用VAD (语音活动检测)
    float vad_threshold;         // VAD阈值 (0.0-1.0)
    bool aec_enable;            // 是否启用回声消除
    bool aec_filter;            // 是否启用AEC滤波器
} wakenet_config_t;

// ==================== 初始化和配置 ====================

/**
 * @brief 初始化唤醒词检测模块
 * @param config 唤醒词配置
 * @param event_cb 事件回调函数
 * @param user_data 用户数据
 * @return ESP_OK成功
 */
esp_err_t wakenet_init(const wakenet_config_t *config, wakenet_event_callback_t event_cb, void *user_data);

/**
 * @brief 反初始化唤醒词检测模块
 * @return ESP_OK成功
 */
esp_err_t wakenet_deinit(void);

// ==================== 控制接口 ====================

/**
 * @brief 开始监听唤醒词
 * @return ESP_OK成功
 */
esp_err_t wakenet_start(void);

/**
 * @brief 停止监听唤醒词
 * @return ESP_OK成功
 */
esp_err_t wakenet_stop(void);

/**
 * @brief 处理音频数据（由audio_manager调用）
 * @param data 音频数据(PCM 16-bit mono, 16kHz)
 * @param len 数据长度（样本数）
 * @return ESP_OK成功
 */
esp_err_t wakenet_process_audio(const int16_t *data, size_t len);

// ==================== 状态查询 ====================

/**
 * @brief 获取唤醒词检测状态
 * @return 唤醒词状态
 */
wakenet_state_t wakenet_get_state(void);

/**
 * @brief 是否正在监听
 * @return true监听中
 */
bool wakenet_is_listening(void);

/**
 * @brief 获取最后一次检测的置信度
 * @return 置信度 (0.0 - 1.0)
 */
float wakenet_get_last_score(void);

// ==================== 配置 ====================

/**
 * @brief 设置VAD阈值
 * @param threshold VAD阈值 (0.0-1.0)
 * @return ESP_OK成功
 */
esp_err_t wakenet_set_vad_threshold(float threshold);

/**
 * @brief 启用/禁用AEC
 * @param enable true启用
 * @return ESP_OK成功
 */
esp_err_t wakenet_set_aec(bool enable);

#ifdef __cplusplus
}
#endif

#endif // WAKENET_MODULE_H
