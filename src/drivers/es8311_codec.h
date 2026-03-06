/**
 * @file es8311_codec.h
 * @brief ES8311音频编解码器驱动头文件
 */

#ifndef ES8311_CODEC_H
#define ES8311_CODEC_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ES8311状态
 */
typedef enum {
    ES8311_STATE_STOP = 0,
    ES8311_STATE_PLAYING,
    ES8311_STATE_RECORDING
} es8311_state_t;

/**
 * @brief 音频格式
 */
typedef struct {
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    uint8_t channels;
} es8311_format_t;

// ==================== 函数声明 ====================

/**
 * @brief 初始化ES8311编解码器
 * @param format 音频格式
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_init(const es8311_format_t *format);

/**
 * @brief 反初始化ES8311编解码器
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_deinit(void);

/**
 * @brief 启动播放
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_start_play(void);

/**
 * @brief 停止播放
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_stop_play(void);

/**
 * @brief 启动录音
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_start_record(void);

/**
 * @brief 停止录音
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_stop_record(void);

/**
 * @brief 设置音量
 * @param volume 音量(0-100)
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_set_volume(uint8_t volume);

/**
 * @brief 获取音量
 * @param volume 音量指针
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_get_volume(uint8_t *volume);

/**
 * @brief 静音控制
 * @param mute true静音，false取消静音
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_mute(bool mute);

/**
 * @brief 设置采样率
 * @param sample_rate 采样率
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_set_sample_rate(uint32_t sample_rate);

/**
 * @brief 检测ES8311是否存在
 * @return ESP_OK存在，ESP_FAIL不存在
 */
esp_err_t es8311_codec_detect(void);

/**
 * @brief 配置ES8311为播放模式
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_config_play(void);

/**
 * @brief 配置ES8311为录音模式
 * @return ESP_OK成功
 */
esp_err_t es8311_codec_config_record(void);

#ifdef __cplusplus
}
#endif

#endif // ES8311_CODEC_H
