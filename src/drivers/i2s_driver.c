/**
 * @file i2s_driver.c
 * @brief I2S音频驱动实现
 */

#include "i2s_driver.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "I2S_DRV";
static i2s_driver_direction_t g_direction = I2S_DRIVER_DIR_BOTH;
static uint32_t g_current_sample_rate = I2S_SAMPLE_RATE;

// ==================== I2S初始化 ====================

esp_err_t i2s_driver_init(i2s_driver_direction_t direction, const i2s_audio_config_t *config)
{
    ESP_LOGI(TAG, "初始化I2S驱动，方向: %d, 采样率: %luHz",
             direction, config->sample_rate);

    g_direction = direction;
    g_current_sample_rate = config->sample_rate;

    // I2S配置
    i2s_config_t i2s_cfg = {
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .sample_rate = config->sample_rate,
        .bits_per_sample = config->bits_per_sample,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // 单声道(左声道)
        .dma_buf_count = config->dma_buf_count,
        .dma_buf_len = config->dma_buf_len,
        .use_apll = true,                       // 使用APLL获得更准确的时钟
        .tx_desc_auto_clear = true,             // 自动清除TX描述符
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .fixed_mclk = 0,                        // 禁用固定MCLK
        .mclk_multiple = I2S_MCLK_MULTIPLE_256, // MCLK倍频
    };

    // 根据方向配置
    if (direction == I2S_DRIVER_DIR_RX || direction == I2S_DRIVER_DIR_BOTH) {
        i2s_cfg.mode |= I2S_MODE_MASTER;
        i2s_cfg.mode |= I2S_MODE_RX;
    }

    if (direction == I2S_DRIVER_DIR_TX || direction == I2S_DRIVER_DIR_BOTH) {
        i2s_cfg.mode |= I2S_MODE_MASTER;
        i2s_cfg.mode |= I2S_MODE_TX;
    }

    // I2S引脚配置
    i2s_pin_config_t pin_cfg = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCK_PIN,
        .data_out_num = I2S_DIN_PIN,    // ESP32的DOUT连接到ES8311的DIN
        .data_in_num = I2S_DOUT_PIN,    // ESP32的DIN连接到ES8311的DOUT
        .mck_io_num = -1,               // 禁用MCLK (ESP32-WROOM GPIO14不支持MCLK输出)
    };

    // 安装I2S驱动
    esp_err_t ret = i2s_driver_install(I2S_NUM, &i2s_cfg, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置I2S引脚
    ret = i2s_set_pin(I2S_NUM, &pin_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S引脚配置失败: %s", esp_err_to_name(ret));
        i2s_driver_uninstall(I2S_NUM);
        return ret;
    }

    ESP_LOGI(TAG, "I2S驱动初始化完成");
    return ESP_OK;
}

// ==================== I2S反初始化 ====================

esp_err_t i2s_driver_deinit(void)
{
    ESP_LOGI(TAG, "反初始化I2S驱动");

    esp_err_t ret = i2s_driver_uninstall(I2S_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S驱动卸载失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2S驱动已卸载");
    return ESP_OK;
}

// ==================== I2S启动/停止 ====================

esp_err_t i2s_driver_start(void)
{
    ESP_LOGD(TAG, "启动I2S");

    esp_err_t ret = i2s_start(I2S_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S启动失败: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t i2s_driver_stop(void)
{
    ESP_LOGD(TAG, "停止I2S");

    esp_err_t ret = i2s_stop(I2S_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S停止失败: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// ==================== I2S数据读写 ====================

int i2s_driver_read(void *buffer, size_t len, uint32_t timeout_ms)
{
    size_t bytes_read = 0;

    esp_err_t ret = i2s_read(I2S_NUM, buffer, len, &bytes_read,
                              pdMS_TO_TICKS(timeout_ms));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S读取失败: %s", esp_err_to_name(ret));
        return -1;
    }

    return (int)bytes_read;
}

int i2s_driver_write(const void *buffer, size_t len, uint32_t timeout_ms)
{
    size_t bytes_written = 0;

    esp_err_t ret = i2s_write(I2S_NUM, buffer, len, &bytes_written,
                               pdMS_TO_TICKS(timeout_ms));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S写入失败: %s", esp_err_to_name(ret));
        return -1;
    }

    return (int)bytes_written;
}

// ==================== I2S采样率设置 ====================

esp_err_t i2s_driver_set_sample_rate(uint32_t sample_rate)
{
    ESP_LOGI(TAG, "设置I2S采样率: %luHz", sample_rate);

    esp_err_t ret = i2s_set_sample_rates(I2S_NUM, sample_rate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S采样率设置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    g_current_sample_rate = sample_rate;
    return ESP_OK;
}

uint32_t i2s_driver_get_sample_rate(void)
{
    return g_current_sample_rate;
}

// ==================== I2S清空缓冲区 ====================

esp_err_t i2s_driver_clear_buffer(void)
{
    ESP_LOGD(TAG, "清空I2S DMA缓冲区");

    esp_err_t ret = i2s_zero_dma_buffer(I2S_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S DMA缓冲区清空失败: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// ==================== I2S重新配置 ====================

esp_err_t i2s_driver_reconfig(uint32_t sample_rate, i2s_driver_direction_t direction)
{
    ESP_LOGI(TAG, "重新配置I2S: 采样率=%luHz, 方向=%d", sample_rate, direction);

    // 停止I2S
    i2s_stop(I2S_NUM);

    // 反初始化I2S
    i2s_driver_uninstall(I2S_NUM);

    // 重新初始化I2S
    i2s_audio_config_t config = {
        .sample_rate = sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE,
        .dma_buf_count = I2S_DMA_BUF_COUNT,
        .dma_buf_len = I2S_DMA_BUF_LEN,
    };

    esp_err_t ret = i2s_driver_init(direction, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S重新配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 启动I2S
    ret = i2s_start(I2S_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S启动失败: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// ==================== I2S调试功能 ====================

/**
 * @brief 读取I2S数据并打印统计信息（用于调试）
 */
esp_err_t i2s_driver_debug_sample(void)
{
    int16_t buffer[512];
    int bytes_read = i2s_driver_read(buffer, sizeof(buffer), 1000);

    if (bytes_read > 0) {
        int samples = bytes_read / 2;
        int32_t sum = 0;
        int16_t min = 32767, max = -32768;

        for (int i = 0; i < samples; i++) {
            sum += abs(buffer[i]);
            if (buffer[i] < min) min = buffer[i];
            if (buffer[i] > max) max = buffer[i];
        }

        int32_t avg = sum / samples;
        ESP_LOGI(TAG, "I2S数据: %d样本, 平均=%ld, 最小=%d, 最大=%d",
                 samples, avg, min, max);
    } else {
        ESP_LOGW(TAG, "I2S读取失败或超时");
    }
    return ESP_OK;
}
