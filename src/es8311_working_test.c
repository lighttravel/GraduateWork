/**
 * @file es8311_working_test.c
 * @brief 基于已知工作的ESP32-WROOM配置的完整测试
 * 包括完整的PLL和时钟配置
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "ES8311_WORKING";

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

static uint8_t es8311_read_reg(uint8_t reg)
{
    uint8_t val;
    i2c_master_transmit_receive(i2c_dev, &reg, 1, &val, 1, pdMS_TO_TICKS(1000));
    return val;
}

// 完整的ES8311初始化序列 - 基于ESP32-WROOM工作配置
static void es8311_full_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(50));

    printf("\n");
    printf("========================================\n");
    printf("  ES8311 Full Initialization\n");
    printf("========================================\n");
    printf("\n");

    vTaskDelay(pdMS_TO_TICKS(50));

    // 复位ES8311
    printf("Step 1: Reset ES8311...\n");
    es8311_write_reg(0x00, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 读取复位后的状态
    uint8_t chip_id = es8311_read_reg(0xFD);
    printf("Chip ID: 0x%02X\n", chip_id);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 基础配置 - 从机模式
    printf("Step 2: Configure slave mode...\n");
    es8311_write_reg(0x00, 0x3F);  // Slave mode, normal operation
    vTaskDelay(pdMS_TO_TICKS(10));

    // 时钟和PLL配置
    printf("Step 3: Configure clocks and PLL...\n");
    es8311_write_reg(0x02, 0xF0);  // MCLK input, PLL off, BCLK divider = 1
    es8311_write_reg(0x03, 0x17);  // ADC OSR=64x, DAC OSR=128x
    es8311_write_reg(0x04, 0x00);  // DAC control
    es8311_write_reg(0x05, 0x00);  // ADC control
    es8311_write_reg(0x06, 0x00);  // ADC/DAC clock divider
    vTaskDelay(pdMS_TO_TICKS(10));

    // I2S接口配置
    printf("Step 4: Configure I2S interface...\n");
    es8311_write_reg(0x08, 0x20);  // DAC interface control
    es8311_write_reg(0x09, 0x0C);  // I2S format, 16-bit
    vTaskDelay(pdMS_TO_TICKS(10));

    // DAC电源和输出路径
    printf("Step 5: Power up DAC and enable output...\n");
    es8311_write_reg(0x2A, 0x04);  // DAC power - enable DAC reference
    es8311_write_reg(0x2B, 0x00);  // DAC control
    es8311_write_reg(0x2C, 0x10);  // LOUT2 volume control initial
    es8311_write_reg(0x2D, 0x10);  // LOUT1 volume control initial
    vTaskDelay(pdMS_TO_TICKS(50));

    // 输出使能
    printf("Step 6: Enable outputs...\n");
    es8311_write_reg(0x12, 0x10);  // LOUT2 enable
    es8311_write_reg(0x13, 0x10);  // LOUT1 enable
    vTaskDelay(pdMS_TO_TICKS(10));

    // 设置音量和取消静音
    printf("Step 7: Set volume and unmute...\n");
    es8311_write_reg(0x2F, 0x2F);  // Volume (maximum)
    es8311_write_reg(0x32, 0x00);  // Unmute
    vTaskDelay(pdMS_TO_TICKS(10));

    // 验证关键寄存器
    printf("\n");
    printf("========================================\n");
    printf("  Register Verification\n");
    printf("========================================\n");
    printf("\n");

    uint8_t reg00 = es8311_read_reg(0x00);
    uint8_t reg02 = es8311_read_reg(0x02);
    uint8_t reg09 = es8311_read_reg(0x09);
    uint8_t reg12 = es8311_read_reg(0x12);
    uint8_t reg13 = es8311_read_reg(0x13);
    uint8_t reg2F = es8311_read_reg(0x2F);
    uint8_t reg32 = es8311_read_reg(0x32);

    printf("REG00 (Control): 0x%02X %s\n", reg00, (reg00 == 0x3F) ? "[OK]" : "[!]");
    printf("REG02 (Clock):   0x%02X %s\n", reg02, (reg02 == 0xF0) ? "[OK]" : "[!]");
    printf("REG09 (I2S):     0x%02X %s\n", reg09, (reg09 == 0x0C) ? "[OK]" : "[!]");
    printf("REG12 (LOUT2):   0x%02X %s\n", reg12, (reg12 & 0x10) ? "[ENABLED]" : "[DISABLED!]");
    printf("REG13 (LOUT1):   0x%02X %s\n", reg13, (reg13 & 0x10) ? "[ENABLED]" : "[DISABLED!]");
    printf("REG2F (Volume):  0x%02X\n", reg2F);
    printf("REG32 (Mute):    0x%02X %s\n", reg32, (reg32 == 0x00) ? "[UNMUTED]" : "[MUTED!]");

    printf("\n[OK] ES8311 initialization complete\n");
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
    printf("\n");
    printf("========================================\n");
    printf("  I2S Initialization\n");
    printf("========================================\n");
    printf("\n");

    printf("I2S Configuration:\n");
    printf("  MCLK: GPIO%d\n", I2S_MCLK_IO);
    printf("  BCLK: GPIO%d\n", I2S_BCLK_IO);
    printf("  WS:   GPIO%d\n", I2S_WS_IO);
    printf("  DOUT: GPIO%d\n", I2S_DOUT_IO);
    printf("\n");

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
        printf("Failed to create I2S channel: %d\n", ret);
        return ret;
    }

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
        printf("Failed to init I2S std mode: %d\n", ret);
        return ret;
    }

    ret = i2s_channel_enable(tx_handle);
    if (ret == ESP_OK) {
        printf("[OK] I2S initialized successfully\n");
    }

    return ret;
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

static void play_test_tone(void)
{
    const int duration_ms = 3000;
    const int total_samples = (EXAMPLE_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buffer = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));

    if (!buffer) {
        printf("Failed to allocate buffer\n");
        return;
    }

    printf("Generating 1kHz test tone (%d seconds)...\n", duration_ms / 1000);
    generate_1khz_sine(buffer, total_samples);

    printf("Sending to I2S...\n");
    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(
        tx_handle,
        (uint8_t *)buffer,
        total_samples * 2 * sizeof(int16_t),
        &bytes_written,
        pdMS_TO_TICKS(10000)
    );

    if (ret == ESP_OK) {
        printf("[OK] Sent %d bytes\n", bytes_written);
        printf("You should hear a 1kHz tone!\n");
    } else {
        printf("[ERROR] I2S write failed: %d\n", ret);
    }

    free(buffer);
}

void app_main(void)
{
    // 立即让出CPU，避免看门狗触发
    vTaskDelay(pdMS_TO_TICKS(100));

    printf("\n");
    printf("========================================\n");
    printf("  ES8311 Working Configuration Test\n");
    printf("  Based on ESP32-WROOM setup\n");
    printf("========================================\n");
    printf("\n");

    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize I2C
    printf("========================================\n");
    printf("  I2C Initialization\n");
    printf("========================================\n");
    printf("\n");

    if (init_i2c() != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }
    printf("[OK] I2C initialized\n");

    vTaskDelay(pdMS_TO_TICKS(500));

    // Full ES8311 initialization
    es8311_full_init();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Initialize I2S
    if (init_i2s() != ESP_OK) {
        printf("[ERROR] I2S init failed\n");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    // Play test tone in loop
    int test = 0;
    while (1) {
        printf("\n");
        printf("========================================\n");
        printf("[Test Round %d]\n", ++test);
        printf("========================================\n");
        printf("\n");

        play_test_tone();

        printf("\nWaiting 5 seconds before next test...\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
