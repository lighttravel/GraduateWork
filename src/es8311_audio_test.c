/**
 * @file es8311_audio_test.c
 * @brief ES8311音频输出测试 - 基于立创开源项目配置
 * 参考: https://github.com/linarobot/ESP32-S3-Box
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "AUDIO_TEST";

// Moji官方引脚配置
#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define I2S_MCLK_IO        GPIO_NUM_6
#define I2S_BCLK_IO        GPIO_NUM_14
#define I2S_WS_IO          GPIO_NUM_12
#define I2S_DO_IO          GPIO_NUM_13

#define EXAMPLE_SAMPLE_RATE     16000
#define EXAMPLE_MCLK_MULTIPLE   256
#define ES8311_ADDR             0x18

static i2s_chan_handle_t tx_handle = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;

// 读写ES8311寄存器
static esp_err_t es8311_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_transmit(i2c_dev, buf, 2, pdMS_TO_TICKS(1000));
}

static esp_err_t es8311_read_reg(uint8_t reg, uint8_t *data)
{
    return i2c_master_transmit_receive(i2c_dev, &reg, 1, data, 1, pdMS_TO_TICKS(1000));
}

// ES8311初始化序列 - 基于立创开源项目
static esp_err_t es8311_init(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311 Initialization\n");
    printf("  Based on Licat ES8311 config\n");
    printf("========================================\n");
    printf("\n");

    esp_err_t ret = ESP_OK;

    // 复位ES8311
    printf("[1/8] Reset ES8311...\n");
    ret |= es8311_write_reg(0x00, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 设置ADC/DAC时钟和运行模式
    printf("[2/8] Configure clock system...\n");
    ret |= es8311_write_reg(0x02, 0x00);  // 正常运行
    ret |= es8311_write_reg(0x03, 0x07);  // ADC OSR=64x
    ret |= es8311_write_reg(0x04, 0x00);  // 从机模式

    // 配置主时钟
    printf("[3/8] Configure MCLK...\n");
    ret |= es8311_write_reg(0x05, 0x00);  // 从机模式
    ret |= es8311_write_reg(0x06, 0x00);  // MCLK=256fs

    // 配置DAC和ADC的串行音频接口
    printf("[4/8] Configure audio interface...\n");
    ret |= es8311_write_reg(0x09, 0x0C);  // DAC: I2S, 16-bit
    ret |= es8311_write_reg(0x0A, 0x0C);  // ADC: I2S, 16-bit
    ret |= es8311_write_reg(0x0B, 0x0C);  // ADC: I2S, 16-bit

    // 配置ADC和DAC系统初始化
    printf("[5/8] Initialize ADC/DAC systems...\n");
    ret |= es8311_write_reg(0x14, 0x1A);  // PGA: +6dB
    ret |= es8311_write_reg(0x15, 0x04);  // DMIC: 单端模式
    ret |= es8311_write_reg(0x16, 0x00);  // ADC使能
    ret |= es8311_write_reg(0x17, 0x3C);  // DAC选择
    ret |= es8311_write_reg(0x18, 0x00);  // 两个DAC通道都不静音

    // 配置DAC输出
    printf("[6/8] Configure DAC output...\n");
    ret |= es8311_write_reg(0x19, 0x00);  // 左DAC音量
    ret |= es8311_write_reg(0x1A, 0x00);  // 右DAC音量

    // 配置输出路径
    printf("[7/8] Configure output path...\n");
    ret |= es8311_write_reg(0x2B, 0x00);  // 开启DAC电源
    ret |= es8311_write_reg(0x2F, 0x2F);  // 左DAC数字音量最大
    ret |= es8311_write_reg(0x30, 0x2F);  // 右DAC数字音量最大

    // 取消静音
    printf("[8/8] Unmute DAC...\n");
    ret |= es8311_write_reg(0x32, 0x00);  // 取消DAC静音

    // 读取并验证关键寄存器
    printf("\nVerifying configuration...\n");
    uint8_t val;

    es8311_read_reg(0x00, &val);
    printf("  REG00 = 0x%02X (should be 0x00)\n", val);

    es8311_read_reg(0x09, &val);
    printf("  REG09 = 0x%02X (should be 0x0C)\n", val);

    es8311_read_reg(0x2F, &val);
    printf("  REG2F = 0x%02X (should be 0x2F)\n", val);

    es8311_read_reg(0x32, &val);
    printf("  REG32 = 0x%02X (should be 0x00)\n", val);

    if (ret == ESP_OK) {
        printf("\n[OK] ES8311 initialized successfully\n");
    } else {
        printf("\n[FAIL] ES8311 initialization had errors\n");
    }

    printf("========================================\n");
    printf("\n");

    return ret;
}

// 初始化I2C
static esp_err_t init_i2c(void)
{
    printf("Initializing I2C (SCL=GPIO%d, SDA=GPIO%d)...\n", I2C_SCL_PIN, I2C_SDA_PIN);

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

    ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev);
    if (ret == ESP_OK) {
        printf("[OK] I2C initialized\n\n");
    }

    return ret;
}

// 初始化I2S
static esp_err_t init_i2s(void)
{
    printf("Initializing I2S...\n");

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
    if (ret != ESP_OK) return ret;

    ret = i2s_channel_enable(tx_handle);
    if (ret == ESP_OK) {
        printf("[OK] I2S initialized\n\n");
    }

    return ret;
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
        buffer[i * 2] = sample;      // 左声道
        buffer[i * 2 + 1] = sample;  // 右声道
    }
}

// 播放1kHz测试音
static void play_1khz_tone(void)
{
    const int duration_ms = 2000;
    const int total_samples = (EXAMPLE_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buffer = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));

    if (!buffer) {
        printf("[ERROR] Memory allocation failed\n");
        return;
    }

    generate_1khz_sine(buffer, total_samples);

    printf("Playing 1kHz tone for %d seconds...\n", duration_ms / 1000);
    printf("You should hear a clear 1kHz tone!\n\n");

    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(
        tx_handle,
        (uint8_t *)buffer,
        total_samples * 2 * sizeof(int16_t),
        &bytes_written,
        pdMS_TO_TICKS(5000)
    );

    if (ret == ESP_OK) {
        printf("[OK] Sent %d bytes of audio data\n", bytes_written);
    } else {
        printf("[ERROR] I2S write failed\n");
    }

    free(buffer);
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311 Audio Output Test\n");
    printf("  Licat-based configuration\n");
    printf("========================================\n");
    printf("\n");

    // 初始化I2C
    if (init_i2c() != ESP_OK) {
        printf("[ERROR] I2C initialization failed\n");
        return;
    }

    // 初始化ES8311
    if (es8311_init() != ESP_OK) {
        printf("[ERROR] ES8311 initialization failed\n");
        return;
    }

    // 初始化I2S
    if (init_i2s() != ESP_OK) {
        printf("[ERROR] I2S initialization failed\n");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    printf("\n");
    printf("========================================\n");
    printf("  Starting Audio Playback Test\n");
    printf("========================================\n");
    printf("\n");

    // 持续播放1kHz音调
    int count = 0;
    while (1) {
        printf("\n[Test %d]\n", ++count);
        play_1khz_tone();
        printf("\nWaiting 2 seconds before next test...\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
