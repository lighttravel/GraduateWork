/**
 * @file es8311_full_config.c
 * @brief ES8311完整配置 - 参考数据手册和立创例程
 *
 * 配置所有必要的ES8311寄存器，包括DAC输出和功放使能
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "ES8311_FULL";

// Moji小智AI官方引脚配置
#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define I2C_NUM            I2C_NUM_0
#define I2C_FREQ_HZ        100000

#define I2S_MCLK_IO        GPIO_NUM_6
#define I2S_BCLK_IO        GPIO_NUM_14
#define I2S_WS_IO          GPIO_NUM_12
#define I2S_DO_IO          GPIO_NUM_13

#define EXAMPLE_SAMPLE_RATE     16000
#define EXAMPLE_MCLK_MULTIPLE   256
#define EXAMPLE_VOICE_VOLUME    70

// ES8311寄存器地址
#define ES8311_REG00       0x00    // 软位控制
#define ES8311_REG02       0x02    // 时钟管理
#define ES8311_REG03       0x03    // 时钟管理
#define ES8311_REG04       0x04    // 时钟管理
#define ES8311_REG05       0x05    // 时钟管理
#define ES8311_REG06       0x06    // 时钟管理
#define ES8311_REG09       0x09    // DAC数据接口
#define ES8311_REG0A       0x0A    // ADC数据接口
#define ES8311_REG0B       0x0B    // ADC数据接口
#define ES8311_REG0C       0x0C    // ADC数据接口
#define ES8311_REG14       0x14    // ADC控制
#define ES8311_REG16       0x16    // ADC控制
#define ES8311_REG2B       0x2B    // DAC电源
#define ES8311_REG2F       0x2F    // DAC音量
#define ES8311_REG32       0x32    // DAC静音
#define ES8311_ADDR        0x18    // ES8311 I2C地址

static i2s_chan_handle_t tx_handle = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;

// 读ES8311寄存器
static esp_err_t es8311_read_reg(uint8_t reg, uint8_t *data)
{
    return i2c_master_transmit_receive(i2c_dev, &reg, 1, data, 1, pdMS_TO_TICKS(1000));
}

// 写ES8311寄存器并验证
static esp_err_t es8311_write_reg_verify(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};

    // 写入
    esp_err_t ret = i2c_master_transmit(i2c_dev, buf, 2, pdMS_TO_TICKS(1000));

    if (ret == ESP_OK) {
        // 读取验证
        uint8_t read_val = 0xFF;
        esp_err_t read_ret = es8311_read_reg(reg, &read_val);

        if (read_ret == ESP_OK) {
            if (read_val == data) {
                ESP_LOGI(TAG, "✅ [0x%02X] = 0x%02X (验证成功)", reg, data);
            } else {
                ESP_LOGW(TAG, "⚠️  [0x%02X] 写入0x%02X, 读取0x%02X (不匹配)", reg, data, read_val);
            }
        } else {
            ESP_LOGE(TAG, "❌ [0x%02X] 写入成功但读取失败", reg);
        }
    } else {
        ESP_LOGE(TAG, "❌ [0x%02X] 写入失败: %s", reg, esp_err_to_name(ret));
    }

    return ret;
}

// 配置ES8311
static esp_err_t config_es8311_full(void)
{
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "完整配置ES8311 - 带寄存器验证");
    ESP_LOGI(TAG, "========================================");

    esp_err_t ret = ESP_OK;

    // 1. 软复位
    ESP_LOGI(TAG, "步骤1: 软复位...");
    ret |= es8311_write_reg_verify(ES8311_REG00, 0x80);  // 软复位
    vTaskDelay(pdMS_TO_TICKS(100));

    // 2. 设置为从机模式，从BCLK输入
    ESP_LOGI(TAG, "\n步骤2: 设置从机模式...");
    uint8_t reg00 = 0;
    es8311_read_reg(ES8311_REG00, &reg00);
    reg00 &= 0xBF;  // bit6 = 0 (从机模式)
    ret |= es8311_write_reg_verify(ES8311_REG00, reg00);
    ESP_LOGI(TAG, "[0x00] = 0x%02X (从机模式)", reg00);

    // 3. 配置时钟系统
    ESP_LOGI(TAG, "\n步骤3: 配置时钟系统...");
    ret |= es8311_write_reg_verify(ES8311_REG02, 0x00);  // PLL使能
    ret |= es8311_write_reg_verify(ES8311_REG03, 0x17);  // ADC OSR 64x, DAC OSR 128x
    ret |= es8311_write_reg_verify(ES8311_REG04, 0x00);  // 从机模式
    ret |= es8311_write_reg_verify(ES8311_REG05, 0x00);  // 从机模式
    ret |= es8311_write_reg_verify(ES8311_REG06, 0x00);  // BCLK 256fs
    ESP_LOGI(TAG, "时钟配置完成");

    // 4. 配置DAC数据接口
    ESP_LOGI(TAG, "\n步骤4: 配置DAC数据接口...");
    ret |= es8311_write_reg_verify(ES8311_REG09, 0x0C);  // 16-bit I2S格式
    ESP_LOGI(TAG, "[0x09] = 0x0C (16-bit I2S格式)");

    // 5. 配置ADC数据接口
    ESP_LOGI(TAG, "\n步骤5: 配置ADC数据接口...");
    ret |= es8311_write_reg_verify(ES8311_REG0A, 0x0C);  // 16-bit I2S格式
    ret |= es8311_write_reg_verify(ES8311_REG0B, 0x0C);  // 16-bit I2S格式
    ret |= es8311_write_reg_verify(ES8311_REG0C, 0x0C);  // 16-bit I2S格式
    ESP_LOGI(TAG, "ADC接口配置完成");

    // 6. 配置ADC控制
    ESP_LOGI(TAG, "\n步骤6: 配置ADC控制...");
    ret |= es8311_write_reg_verify(ES8311_REG14, 0x03);  // 左通道选择
    ret |= es8311_write_reg_verify(ES8311_REG16, 0x06);  // 使能ADC
    ESP_LOGI(TAG, "ADC控制完成");

    // 7. 配置DAC电源和音量
    ESP_LOGI(TAG, "\n步骤7: 配置DAC电源和音量...");
    ret |= es8311_write_reg_verify(ES8311_REG2B, 0x00);  // DAC电源开启
    ret |= es8311_write_reg_verify(ES8311_REG2F, 0x2F);  // 最大音量
    ESP_LOGI(TAG, "DAC电源配置完成");

    // 8. 取消DAC静音
    ESP_LOGI(TAG, "\n步骤8: 取消DAC静音...");
    ret |= es8311_write_reg_verify(ES8311_REG32, 0x00);  // 取消静音
    ESP_LOGI(TAG, "[0x32] = 0x00 (取消静音)");

    // 9. 配置更多寄存器（DAC输出路径）
    ESP_LOGI(TAG, "\n步骤9: 配置DAC输出路径...");
    ret |= es8311_write_reg_verify(0x10, 0x00);  // LOUT MUX使能
    ret |= es8311_write_reg_verify(0x12, 0x00);  // LOUT2使能
    ret |= es8311_write_reg_verify(0x13, 0x00);  // LOUT1使能
    ret |= es8311_write_reg_verify(0x25, 0x03);  // LOUT1 MUX选择
    ret |= es8311_write_reg_verify(0x27, 0x0C);  // LOUT MUX增益
    ESP_LOGI(TAG, "DAC输出路径配置完成");

    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "✅ ES8311完整配置完成！");
    ESP_LOGI(TAG, "========================================\n");

    return ret;
}

// 初始化I2C
static esp_err_t init_i2c(void)
{
    ESP_LOGI(TAG, "初始化I2C...");

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM,
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
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C设备添加失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✅ I2C初始化成功");
    return ESP_OK;
}

// 初始化I2S
static esp_err_t init_i2s(void)
{
    ESP_LOGI(TAG, "初始化I2S...");

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

    ESP_LOGI(TAG, "✅ I2S初始化成功");
    return ESP_OK;
}

// 生成正弦波
static void generate_sine_wave(int16_t *buffer, int samples, int frequency)
{
    const float amplitude = 20000.0f;
    const float sample_period = 1.0f / EXAMPLE_SAMPLE_RATE;

    for (int i = 0; i < samples; i++) {
        float t = i * sample_period;
        buffer[i * 2] = (int16_t)(amplitude * sinf(2.0f * M_PI * frequency * t));
        buffer[i * 2 + 1] = buffer[i * 2];
    }
}

// 播放测试音
static void play_test_tone(int frequency, int duration_ms)
{
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
        ESP_LOGI(TAG, "🔊 如果配置正确，应该听到清晰的%dHz音调!", frequency);
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
    printf("  ES8311完整配置测试\n");
    printf("  参考ES8311数据手册\n");
    printf("========================================\n");
    printf("\n");

    ESP_LOGI(TAG, "说明：");
    ESP_LOGI(TAG, "  本程序手动配置ES8311所有关键寄存器");
    ESP_LOGI(TAG, "  包括DAC输出路径和音频路径配置");
    ESP_LOGI(TAG, "");

    // 初始化I2C
    if (init_i2c() != ESP_OK) {
        ESP_LOGE(TAG, "I2C初始化失败");
        return;
    }

    // 配置ES8311
    if (config_es8311_full() != ESP_OK) {
        ESP_LOGE(TAG, "ES8311配置失败");
        return;
    }

    // 初始化I2S
    if (init_i2s() != ESP_OK) {
        ESP_LOGE(TAG, "I2S初始化失败");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "开始循环播放1kHz测试音");
    ESP_LOGI(TAG, "========================================");

    // 持续播放1kHz
    while (1) {
        play_test_tone(1000, 1000);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
