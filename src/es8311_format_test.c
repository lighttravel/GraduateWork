/**
 * @file es8311_format_test.c
 * @brief ES8311测试多种I2S格式配置
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "FORMAT_TEST";

#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define I2S_MCLK_IO        GPIO_NUM_6
#define I2S_BCLK_IO        GPIO_NUM_14
#define I2S_WS_IO          GPIO_NUM_12
#define I2S_DOUT_IO        GPIO_NUM_11  // 用户实际接线

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

// ES8311基本初始化
static void es8311_basic_init(void)
{
    printf("Initializing ES8311 (basic)...\n");

    es8311_write_reg(0x00, 0x80);  // 复位
    vTaskDelay(pdMS_TO_TICKS(100));

    es8311_write_reg(0x00, 0x00);  // 正常运行
    es8311_write_reg(0x02, 0x00);  // 时钟使能
    es8311_write_reg(0x09, 0x0C);  // DAC: I2S 16-bit
    es8311_write_reg(0x2B, 0x00);  // DAC电源
    es8311_write_reg(0x2F, 0x2F);  // 音量最大
    es8311_write_reg(0x32, 0x00);  // 取消静音

    printf("ES8311 basic init done\n\n");
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

// 生成1kHz正弦波
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

// 测试一种I2S格式配置
static void test_i2s_format(int test_num, bool bit_shift, bool ws_pol_inv)
{
    printf("\n========================================\n");
    printf("Test %d: I2S Format Configuration\n", test_num);
    printf("========================================\n");
    printf("bit_shift = %s\n", bit_shift ? "TRUE" : "FALSE");
    printf("ws_pol_inv = %s\n", ws_pol_inv ? "TRUE" : "FALSE");
    printf("\n");

    // 重新初始化ES8311
    es8311_basic_init();

    // 如果通道已存在，先删除
    if (tx_handle != NULL) {
        i2s_channel_disable(tx_handle);
        i2s_del_channel(tx_handle);
        tx_handle = NULL;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 创建I2S通道
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
        printf("[ERROR] Failed to create I2S channel\n");
        return;
    }

    // 配置I2S标准模式
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
            .ws_pol = ws_pol_inv,
            .bit_shift = bit_shift,
        },
        .gpio_cfg = {
            .mclk = I2S_MCLK_IO,
            .bclk = I2S_BCLK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DOUT_IO,
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
        printf("[ERROR] Failed to init I2S\n");
        return;
    }

    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        printf("[ERROR] Failed to enable I2S\n");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    // 播放测试音
    printf("Playing 1kHz tone for 2 seconds...\n");
    printf("LISTEN CAREFULLY!\n\n");

    const int duration_ms = 2000;
    const int total_samples = (EXAMPLE_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buffer = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));

    if (buffer) {
        generate_1khz_sine(buffer, total_samples);

        size_t bytes_written;
        ret = i2s_channel_write(
            tx_handle,
            (uint8_t *)buffer,
            total_samples * 2 * sizeof(int16_t),
            &bytes_written,
            pdMS_TO_TICKS(5000)
        );

        if (ret == ESP_OK) {
            printf("Sent %d bytes\n", bytes_written);
            printf("Did you hear the tone?\n");
        } else {
            printf("[ERROR] I2S write failed\n");
        }

        free(buffer);
    }

    // 等待音频播放完成
    vTaskDelay(pdMS_TO_TICKS(2500));
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311 I2S Format Test\n");
    printf("  Testing different I2S configurations\n");
    printf("========================================\n");
    printf("\n");

    if (init_i2c() != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }

    printf("[OK] I2C initialized\n");
    printf("ES8311 found at 0x18\n\n");

    // 测试不同的I2S格式组合
    int test = 0;

    // 格式1: Philips I2S标准 (bit_shift=TRUE, ws_pol=FALSE)
    test_i2s_format(++test, true, false);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 格式2: bit_shift关闭 (bit_shift=FALSE, ws_pol=FALSE)
    test_i2s_format(++test, false, false);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 格式3: WS极性反转 (bit_shift=TRUE, ws_pol=TRUE)
    test_i2s_format(++test, true, true);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 格式4: 两者都改变 (bit_shift=FALSE, ws_pol=TRUE)
    test_i2s_format(++test, false, true);
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("\n========================================\n");
    printf("All format tests completed!\n");
    printf("If you heard sound in any test, note which one.\n");
    printf("========================================\n");

    // 循环播放最常用的格式
    printf("\nLooping Test 1 (Standard I2S)...\n\n");

    while (1) {
        test_i2s_format(1, true, false);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
