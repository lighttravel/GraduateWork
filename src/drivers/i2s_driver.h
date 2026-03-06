/**
 * @file i2s_driver.h
 * @brief I2S音频驱动头文件
 */

#ifndef I2S_DRIVER_H
#define I2S_DRIVER_H

#include "driver/i2s.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I2S方向 (使用ESP-IDF的i2s_mode_t)
 */
typedef enum {
    I2S_DRIVER_DIR_TX = 0,     // 发送(播放)
    I2S_DRIVER_DIR_RX,         // 接收(录音)
    I2S_DRIVER_DIR_BOTH        // 全双工
} i2s_driver_direction_t;

/**
 * @brief 音频配置
 */
typedef struct {
    uint32_t sample_rate;
    i2s_bits_per_sample_t bits_per_sample;
    uint32_t dma_buf_count;
    uint32_t dma_buf_len;
} i2s_audio_config_t;

// ==================== 函数声明 ====================

/**
 * @brief 初始化I2S外设
 * @param direction I2S方向
 * @param config 音频配置
 * @return ESP_OK成功
 */
esp_err_t i2s_driver_init(i2s_driver_direction_t direction, const i2s_audio_config_t *config);

/**
 * @brief 反初始化I2S外设
 * @return ESP_OK成功
 */
esp_err_t i2s_driver_deinit(void);

/**
 * @brief 启动I2S
 * @return ESP_OK成功
 */
esp_err_t i2s_driver_start(void);

/**
 * @brief 停止I2S
 * @return ESP_OK成功
 */
esp_err_t i2s_driver_stop(void);

/**
 * @brief 读取音频数据
 * @param buffer 数据缓冲区
 * @param len 缓冲区长度(字节)
 * @param timeout_ms 超时时间(毫秒)
 * @return 实际读取的字节数，错误返回<0
 */
int i2s_driver_read(void *buffer, size_t len, uint32_t timeout_ms);

/**
 * @brief 写入音频数据
 * @param buffer 数据缓冲区
 * @param len 缓冲区长度(字节)
 * @param timeout_ms 超时时间(毫秒)
 * @return 实际写入的字节数，错误返回<0
 */
int i2s_driver_write(const void *buffer, size_t len, uint32_t timeout_ms);

/**
 * @brief 设置采样率
 * @param sample_rate 采样率
 * @return ESP_OK成功
 */
esp_err_t i2s_driver_set_sample_rate(uint32_t sample_rate);

/**
 * @brief 获取采样率
 * @return 当前采样率
 */
uint32_t i2s_driver_get_sample_rate(void);

/**
 * @brief 清空I2S DMA缓冲区
 * @return ESP_OK成功
 */
esp_err_t i2s_driver_clear_buffer(void);

/**
 * @brief 重新配置I2S(用于切换采样率和方向)
 * @param sample_rate 采样率
 * @param direction I2S方向
 * @return ESP_OK成功
 */
esp_err_t i2s_driver_reconfig(uint32_t sample_rate, i2s_driver_direction_t direction);

/**
 * @brief 调试采样 - 读取I2S数据并打印统计信息
 * @return ESP_OK成功
 */
esp_err_t i2s_driver_debug_sample(void);

#ifdef __cplusplus
}
#endif

#endif // I2S_DRIVER_H
