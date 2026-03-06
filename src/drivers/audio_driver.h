/**
 * @file audio_driver.h
 * @brief 统一音频驱动接口
 *
 * 整合 I2C、I2S、ES8311 编解码器的统一接口
 * 使用新版 ESP-IDF API
 */

#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 音频驱动配置结构体
 */
typedef struct {
    // I2C 配置
    gpio_num_t i2c_sda_pin;     ///< I2C SDA 引脚
    gpio_num_t i2c_scl_pin;     ///< I2C SCL 引脚
    uint32_t i2c_freq_hz;       ///< I2C 频率

    // I2S 配置
    gpio_num_t i2s_mclk_pin;    ///< I2S MCLK 引脚
    gpio_num_t i2s_bclk_pin;    ///< I2S BCLK 引脚
    gpio_num_t i2s_ws_pin;      ///< I2S WS/LRCK 引脚
    gpio_num_t i2s_dout_pin;    ///< I2S 数据输出引脚
    gpio_num_t i2s_din_pin;     ///< I2S 数据输入引脚

    // 功放配置
    gpio_num_t pa_pin;          ///< 功放使能引脚
    bool pa_inverted;           ///< PA 使能是否反相

    // 音频配置
    int input_sample_rate;      ///< 输入采样率
    int output_sample_rate;     ///< 输出采样率
    int default_volume;         ///< 默认音量 (0-100)
    float input_gain;           ///< 输入增益 (dB)

    // 功能配置
    bool use_mclk;              ///< 是否使用 MCLK
    bool use_filter;            ///< 是否启用软件低通滤波器 (降噪)
    uint8_t es8311_addr;        ///< ES8311 I2C 地址
} audio_driver_config_t;

/**
 * @brief 音频驱动句柄
 */
typedef struct audio_driver_handle* audio_driver_handle_t;

/**
 * @brief 创建音频驱动
 *
 * 初始化 I2C 总线、I2S 通道和 ES8311 编解码器
 *
 * @param config 配置参数
 * @param p_handle 返回的句柄指针
 * @return esp_err_t
 */
esp_err_t audio_driver_create(const audio_driver_config_t *config,
                               audio_driver_handle_t *p_handle);

/**
 * @brief 销毁音频驱动
 *
 * @param handle 音频驱动句柄
 * @return esp_err_t
 */
esp_err_t audio_driver_destroy(audio_driver_handle_t handle);

/**
 * @brief 启用录音模式
 *
 * @param handle 音频驱动句柄
 * @return esp_err_t
 */
esp_err_t audio_driver_start_recording(audio_driver_handle_t handle);

/**
 * @brief 停止录音模式
 *
 * @param handle 音频驱动句柄
 * @return esp_err_t
 */
esp_err_t audio_driver_stop_recording(audio_driver_handle_t handle);

/**
 * @brief 启用播放模式
 *
 * @param handle 音频驱动句柄
 * @return esp_err_t
 */
esp_err_t audio_driver_start_playback(audio_driver_handle_t handle);

/**
 * @brief 停止播放模式
 *
 * @param handle 音频驱动句柄
 * @return esp_err_t
 */
esp_err_t audio_driver_stop_playback(audio_driver_handle_t handle);

/**
 * @brief 读取音频数据 (从麦克风)
 *
 * @param handle 音频驱动句柄
 * @param buffer 数据缓冲区
 * @param samples 采样点数量
 * @return int 实际读取的采样点数，负数表示错误
 */
int audio_driver_read(audio_driver_handle_t handle, int16_t *buffer, int samples);

/**
 * @brief 写入音频数据 (到扬声器)
 *
 * @param handle 音频驱动句柄
 * @param data 数据缓冲区
 * @param samples 采样点数量
 * @return int 实际写入的采样点数，负数表示错误
 */
int audio_driver_write(audio_driver_handle_t handle, const int16_t *data, int samples);

/**
 * @brief 设置音量
 *
 * @param handle 音频驱动句柄
 * @param volume 音量 (0-100)
 * @return esp_err_t
 */
esp_err_t audio_driver_set_volume(audio_driver_handle_t handle, int volume);

/**
 * @brief 获取音量
 *
 * @param handle 音频驱动句柄
 * @param volume 返回的音量值
 * @return esp_err_t
 */
esp_err_t audio_driver_get_volume(audio_driver_handle_t handle, int *volume);

/**
 * @brief 设置静音
 *
 * @param handle 音频驱动句柄
 * @param mute true=静音
 * @return esp_err_t
 */
esp_err_t audio_driver_set_mute(audio_driver_handle_t handle, bool mute);

/**
 * @brief 检测 ES8311 设备是否存在
 *
 * @param handle 音频驱动句柄
 * @return true 设备存在
 * @return false 设备不存在
 */
bool audio_driver_is_device_ready(audio_driver_handle_t handle);

/**
 * @brief 获取当前采样率
 *
 * @param handle 音频驱动句柄
 * @return int 采样率 (Hz)
 */
int audio_driver_get_sample_rate(audio_driver_handle_t handle);

/**
 * @brief 打印ES8311寄存器诊断信息
 *
 * @param handle 音频驱动句柄
 */
void audio_driver_dump_codec_registers(audio_driver_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_DRIVER_H
