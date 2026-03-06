/**
 * @file es8311_manual_config.c
 * @brief 手动配置ES8311（使用新版I2C master bus API）
 *
 * 避免I2C驱动冲突，直接使用新版I2C API配置ES8311关键寄存器
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "ES8311_MANUAL";

// Moji小智AI官方引脚配置
#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5

#define I2S_MCLK_IO        GPIO_NUM_6
#define I2S_BCLK_IO        GPIO_NUM_14
#define I2S_WS_IO          GPIO_NUM_12
#define I2S_DO_IO          GPIO_NUM_13

#define EXAMPLE_SAMPLE_RATE     16000
#define EXAMPLE_MCLK_MULTIPLE   256

static i2s_chan_handle_t tx_handle = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;

// ES8311寄存器地址
#define ES8311_REG00       0x00    // 复位
#define ES8311_REG02       0x02    // 时钟管理
#define ES8311_REG09       0x09    // DAC数据接口
#define ES8311_REG2B       0x2B    // DAC电源
#define ES8311_REG2F       0x2F    // DAC音量
#define ES8311_REG32       0x32    // DAC静音
#define ES8311_ADDR        0x18    // ES8311 I2C地址

// 写ES8311寄存器
static esp_err_t es8311_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    esp_err_t ret = i2c_master_transmit(i2c_dev, buf, 2, pdMS_TO_TICKS(1000));

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ [0x%02X] = 0x%02X", reg, data);
    } else {
        ESP_LOGE(TAG, "❌ [0x%02X] 写入失败: %s", reg, esp_err_to_name(ret));
    }

    return ret;
}

// 初始化I2C（新版API）
static esp_err_t init_i2c(void)
{
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "初始化I2C（新版master bus API）");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "SCL = GPIO%d, SDA = GPIO%d", I2C_SCL_PIN, I2C_SDA_PIN);

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C总线创建失败: %s", esp_err_to_name(ret));
        return ret;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8311_ADDR,
        .scl_speed_hz = 100000,
    };

    ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C设备添加失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✅ I2C初始化成功\n");
    return ESP_OK;
}

// 手动配置ES8311关键寄存器
static esp_err_t config_es8311(void)
{
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "手动配置ES8311寄存器");
    ESP_LOGI(TAG, "========================================\n");

    esp_err_t ret = ESP_OK;

    // 1. 软复位
    ESP_LOGI(TAG, "步骤1: 软复位ES8311...");
    ret |= es8311_write_reg(ES8311_REG00, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 2. 设置为从机模式
    ESP_LOGI(TAG, "\n步骤2: 设置从机模式...");
    ret |= es8311_write_reg(ES8311_REG00, 0x00);  // 清除复位bit，设置为从机

    // 3. 配置时钟（从机模式，使用BCLK）
    ESP_LOGI(TAG, "\n步骤3: 配置时钟系统...");
    ret |= es8311_write_reg(ES8311_REG02, 0x00);  // 从机模式

    // 4. 配置DAC数据接口（16-bit I2S）
    ESP_LOGI(TAG, "\n步骤4: 配置DAC数据接口...");
    ret |= es8311_write_reg(ES8311_REG09, 0x0C);  // 16-bit I2S格式

    // 5. 设置音量（最大音量）
    ESP_LOGI(TAG, "\n步骤5: 设置音量（最大）...");
    ret |= es8311_write_reg(ES8311_REG2F, 0x2F);  // 最大音量

    // 6. 取消静音
    ESP_LOGI(TAG, "\n步骤6: 取消静音...");
    ret |= es8311_write_reg(ES8311_REG32, 0x00);  // 取消静音

    // 7. 启用DAC电源（关键！）
    ESP_LOGI(TAG, "\n步骤7: 启用DAC电源...");
    ret |= es8311_write_reg(ES8311_REG2B, 0x00);  // DAC电源开启

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "\n========================================");
        ESP_LOGI(TAG, "✅ ES8311配置完成！");
        ESP_LOGI(TAG, "========================================\n");
    } else {
        ESP_LOGE(TAG, "\n❌ ES8311配置失败！\n");
    }

    return ret;
}

// 初始化I2S
static esp_err_t init_i2s(void)
{
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "初始化I2S");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "MCLK = GPIO%d", I2S_MCLK_IO);
    ESP_LOGI(TAG, "BCLK = GPIO%d", I2S_BCLK_IO);
    ESP_LOGI(TAG, "LRCK = GPIO%d", I2S_WS_IO);
    ESP_LOGI(TAG, "DIN  = GPIO%d", I2S_DO_IO);

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 512,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建I2S通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = EXAMPLE_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = EXAMPLE_MCLK_MULTIPLE,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
        },
        .gpio_cfg = {
            .mclk = I2S_MCLK_IO,
            .bclk = I2S_BCLK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DO_IO,
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化I2S失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用I2S失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✅ I2S初始化成功\n");
    return ESP_OK;
}

// 生成正弦波
static void generate_sine_wave(int16_t *buffer, int samples, int frequency) {
    const float amplitude = 20000.0f;
    const float sample_period = 1.0f / EXAMPLE_SAMPLE_RATE;

    for (int i = 0; i < samples; i++) {
        float t = i * sample_period;
        buffer[i * 2] = (int16_t)(amplitude * sinf(2.0f * M_PI * frequency * t));
        buffer[i * 2 + 1] = buffer[i * 2];
    }
}

// 播放测试音
static void play_test_tone(int frequency, int duration_ms) {
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "播放测试音: %dHz, %dms", frequency, duration_ms);
    ESP_LOGI(TAG, "========================================");

    const int total_samples = (EXAMPLE_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buffer = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));

    if (!buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }

    generate_sine_wave(buffer, total_samples, frequency);

    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(
        tx_handle,
        (uint8_t *)buffer,
        total_samples * 2 * sizeof(int16_t),
        &bytes_written,
        pdMS_TO_TICKS(5000)
    );

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 写入 %d 字节音频数据", bytes_written);
        ESP_LOGI(TAG, "🔊 如果配置成功，应该听到清晰的%dHz音调!", frequency);
    } else {
        ESP_LOGE(TAG, "❌ I2S写入失败: %s", esp_err_to_name(ret));
    }

    free(buffer);
    vTaskDelay(pdMS_TO_TICKS(duration_ms + 100));
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311手动配置测试\n");
    printf("  使用新版I2C API手动配置寄存器\n");
    printf("========================================\n");
    printf("\n");

    // 初始化I2C（新版API）
    if (init_i2c() != ESP_OK) {
        ESP_LOGE(TAG, "I2C初始化失败，程序终止");
        return;
    }

    // 手动配置ES8311
    if (config_es8311() != ESP_OK) {
        ESP_LOGE(TAG, "ES8311配置失败，但继续尝试播放...");
    }

    // 初始化I2S
    if (init_i2s() != ESP_OK) {
        ESP_LOGE(TAG, "I2S初始化失败，程序终止");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    // 测试不同频率
    int frequencies[] = {440, 880, 1000, 2000};
    int durations[] = {2000, 1000, 2000, 1000};

    for (int i = 0; i < 4; i++) {
        ESP_LOGI(TAG, "\n【测试 %d/%d】", i+1, 4);
        play_test_tone(frequencies[i], durations[i]);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "所有测试完成!");
    ESP_LOGI(TAG, "========================================");

    // 循环播放
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "\n循环测试: 播放1000Hz音调...");
        play_test_tone(1000, 1000);
    }
}
