/**
 * @file es8311_esp_adf_config.c
 * @brief ES8311配置 - 基于ESP-ADF官方配置
 * 参考: ESP-ADF中的ES8311驱动，用于ESP32-LyraT等官方板
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "ESP_ADF_ES8311";

#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define I2S_MCLK_IO        GPIO_NUM_6
#define I2S_BCLK_IO        GPIO_NUM_14
#define I2S_WS_IO          GPIO_NUM_12
#define I2S_DO_IO          GPIO_NUM_13

#define EXAMPLE_SAMPLE_RATE     16000
#define ES8311_ADDR             0x18

static i2s_chan_handle_t tx_handle = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;

static esp_err_t es8311_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_transmit(i2c_dev, buf, 2, pdMS_TO_TICKS(1000));
}

static esp_err_t es8311_read_reg(uint8_t reg, uint8_t *data)
{
    return i2c_master_transmit_receive(i2c_dev, &reg, 1, data, 1, pdMS_TO_TICKS(1000));
}

// ESP-ADF风格的ES8311初始化
static esp_err_t es8311_init_esp_adf(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311 Init (ESP-ADF style)\n");
    printf("  Based on ESP32-LyraT configuration\n");
    printf("========================================\n");
    printf("\n");

    esp_err_t ret = ESP_OK;

    // 复位ES8311
    printf("[Step 1] Reset...\n");
    es8311_write_reg(0x00, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 设置为从机模式，使用SCLK作为时钟源
    printf("[Step 2] Set slave mode...\n");
    es8311_write_reg(0x00, 0x00);
    es8311_write_reg(0x00, 0x3F);  // 从机模式

    // 配置系统时钟
    printf("[Step 3] Configure system clock...\n");
    es8311_write_reg(0x02, 0xF3);  // PLL关闭
    es8311_write_reg(0x03, 0x00);  // ADC MCLK禁用
    es8311_write_reg(0x04, 0x00);  // DAC MCLK禁用
    es8311_write_reg(0x05, 0x00);  // 从机模式
    es8311_write_reg(0x06, 0x00);  // BCLK=256fs

    // 配置DAC
    printf("[Step 4] Configure DAC...\n");
    es8311_write_reg(0x09, 0x00);  // DAC接口复位
    es8311_write_reg(0x09, 0x6C);  // DAC: I2S, 16-bit, 使能MCLK
    es8311_write_reg(0x02, 0xF0);  // 使能MCLK

    // 配置DAC输出路径和音量
    printf("[Step 5] Configure DAC output path...\n");
    es8311_write_reg(0x17, 0x3C);  // DAC选择
    es8311_write_reg(0x18, 0x00);  // 左DAC不静音
    es8311_write_reg(0x19, 0x00);  // 右DAC不静音
    es8311_write_reg(0x1A, 0x00);  // 左DAC增益
    es8311_write_reg(0x1B, 0x00);  // 右DAC增益

    // 配置DAC电源和音量
    printf("[Step 6] Power on DAC and set volume...\n");
    es8311_write_reg(0x2B, 0x00);  // DAC电源正常
    es8311_write_reg(0x2C, 0x00);  // 耳机放大器正常
    es8311_write_reg(0x2D, 0x01);  // HP输出接地参考
    es8311_write_reg(0x2F, 0x2F);  // 左DAC音量最大
    es8311_write_reg(0x30, 0x2F);  // 右DAC音量最大

    // 取消静音
    printf("[Step 7] Unmute DAC...\n");
    es8311_write_reg(0x32, 0x00);  // DAC不静音

    // 配置LOUT输出（关键！）
    printf("[Step 8] Configure LOUT outputs...\n");
    es8311_write_reg(0x10, 0x00);  // LOUT MUX
    es8311_write_reg(0x12, 0x00);  // LOUT2S
    es8311_write_reg(0x13, 0x00);  // LOUT1
    es8311_write_reg(0x14, 0x1A);  // PGA增益
    es8311_write_reg(0x15, 0x00);  // VMID选择
    es8311_write_reg(0x16, 0x00);  // ADC控制
    es8311_write_reg(0x25, 0x03);  // LOUT1 MUX
    es8311_write_reg(0x26, 0x00);  // MIXLEFT
    es8311_write_reg(0x27, 0x0C);  // LOUT MUX增益

    printf("[Step 9] Verify configuration...\n");
    uint8_t val;
    es8311_read_reg(0x09, &val);
    printf("  REG09 = 0x%02X (DAC interface)\n", val);
    es8311_read_reg(0x2F, &val);
    printf("  REG2F = 0x%02X (Left DAC volume)\n", val);
    es8311_read_reg(0x32, &val);
    printf("  REG32 = 0x%02X (Mute control)\n", val);

    printf("\n[OK] ESP-ADF style init complete\n");
    printf("========================================\n");
    printf("\n");

    return ret;
}

static esp_err_t init_i2c(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus);
    if (ret != ESP_OK) return ret;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8311_ADDR,
        .scl_speed_hz = 100000,
    };

    return i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev);
}

static esp_err_t init_i2s(void)
{
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 512,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) return ret;

    // 使用ESP32-LyraT的标准I2S配置
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = EXAMPLE_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = 256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
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
    if (ret != ESP_OK) return ret;

    return i2s_channel_enable(tx_handle);
}

static void generate_1khz_sine(int16_t *buffer, int samples)
{
    const float amplitude = 32000.0f;
    const float freq = 1000.0f;
    const float sample_period = 1.0f / EXAMPLE_SAMPLE_RATE;

    for (int i = 0; i < samples; i++) {
        float t = i * sample_period;
        int16_t sample = (int16_t)(amplitude * sinf(2.0f * M_PI * freq * t));
        buffer[i * 2] = sample;
        buffer[i * 2 + 1] = sample;
    }
}

static void play_1khz_tone(void)
{
    const int duration_ms = 3000;
    const int total_samples = (EXAMPLE_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buffer = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));

    if (!buffer) return;

    generate_1khz_sine(buffer, total_samples);

    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(
        tx_handle,
        (uint8_t *)buffer,
        total_samples * 2 * sizeof(int16_t),
        &bytes_written,
        pdMS_TO_TICKS(5000)
    );

    if (ret == ESP_OK) {
        printf("Sent %d bytes\n", bytes_written);
    }

    free(buffer);
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311 Audio Test (ESP-ADF config)\n");
    printf("  Using verified LyraT configuration\n");
    printf("========================================\n");
    printf("\n");

    if (init_i2c() != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }

    printf("[OK] I2C initialized\n");

    if (es8311_init_esp_adf() != ESP_OK) {
        printf("[ERROR] ES8311 init failed\n");
        return;
    }

    if (init_i2s() != ESP_OK) {
        printf("[ERROR] I2S init failed\n");
        return;
    }

    printf("[OK] I2S initialized\n");

    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("\n========================================\n");
    printf("Playing continuous 1kHz tone\n");
    printf("========================================\n");
    printf("\n");

    int count = 0;
    while (1) {
        printf("\n[Play %d] - 3 seconds of 1kHz...\n", ++count);
        play_1khz_tone();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
