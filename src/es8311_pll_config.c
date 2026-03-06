/**
 * @file es8311_pll_config.c
 * @brief ES8311完整PLL和时钟配置（基于ESP32-WROOM工作配置）
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "PLL_CONFIG";

// 用户实际接线
#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define I2S_MCLK_IO        GPIO_NUM_6
#define I2S_BCLK_IO        GPIO_NUM_14
#define I2S_WS_IO          GPIO_NUM_12
#define I2S_DO_IO          GPIO_NUM_11

#define EXAMPLE_SAMPLE_RATE     16000
#define ES8311_ADDR             0x18

static i2s_chan_handle_t tx_handle = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;

static esp_err_t es8311_read_reg(uint8_t reg, uint8_t *data)
{
    return i2c_master_transmit_receive(i2c_dev, &reg, 1, data, 1, pdMS_TO_TICKS(1000));
}

static esp_err_t es8311_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    esp_err_t ret = i2c_master_transmit(i2c_dev, buf, 2, pdMS_TO_TICKS(1000));

    if (ret == ESP_OK) {
        // 验证写入
        uint8_t val;
        es8311_read_reg(reg, &val);
        if (val != data) {
            printf("[WARN] REG[0x%02X] wrote 0x%02X, read 0x%02X\n", reg, data, val);
        }
    }

    return ret;
}

// 完整的ES8311初始化，包括PLL配置
static esp_err_t es8311_init_with_pll(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311 Full Init with PLL Config\n");
    printf("  Based on ESP32-WROOM working config\n");
    printf("========================================\n");
    printf("\n");

    esp_err_t ret = ESP_OK;

    // 步骤1: 软复位
    printf("[Step 1] Software reset...\n");
    ret |= es8311_write_reg(0x00, 0x80);  // 复位
    vTaskDelay(pdMS_TO_TICKS(200));
    printf("  Done.\n");

    // 步骤2: 基本控制寄存器设置
    printf("[Step 2] Basic control setup...\n");
    ret |= es8311_write_reg(0x00, 0x3F);  // 从机模式，芯片使能

    // 步骤3: PLL和时钟配置（关键！）
    printf("[Step 3] *** PLL and Clock Configuration ***\n");

    // 时钟和PLL配置
    ret |= es8311_write_reg(0x02, 0xF3);  // 关闭PLL，使用外部MCLK
    vTaskDelay(pdMS_TO_TICKS(10));

    // 或者尝试使用PLL
    // ret |= es8311_write_reg(0x02, 0x00);  // 打开PLL

    // 配置ADC/DAC过采样率
    ret |= es8311_write_reg(0x03, 0x17);  // ADC OSR=64x, DAC OSR=128x

    // 配置主时钟模式
    ret |= es8311_write_reg(0x04, 0x00);  // 从机模式
    ret |= es8311_write_reg(0x05, 0x00);  // 从机模式
    ret |= es8311_write_reg(0x06, 0x00);  // BCLK = 256fs (用于16kHz)

    // 再次设置时钟
    ret |= es8311_write_reg(0x02, 0xF0);  // 使用MCLK，PLL关闭

    printf("  Clock config: MCLK input, PLL off\n");

    // 步骤4: DAC接口配置
    printf("[Step 4] DAC interface configuration...\n");
    ret |= es8311_write_reg(0x09, 0x0C);  // I2S, 16-bit
    printf("  DAC interface: I2S 16-bit\n");

    // 步骤5: ADC接口配置
    printf("[Step 5] ADC interface configuration...\n");
    ret |= es8311_write_reg(0x0A, 0x0C);  // I2S, 16-bit
    ret |= es8311_write_reg(0x0B, 0x0C);  // I2S, 16-bit
    ret |= es8311_write_reg(0x0C, 0x0C);  // I2S, 16-bit

    // 步骤6: ADC控制配置
    printf("[Step 6] ADC control configuration...\n");
    ret |= es8311_write_reg(0x14, 0x1A);  // PGA增益 +6dB
    ret |= es8311_write_reg(0x15, 0x04);  // DMIC单端模式
    ret |= es8311_write_reg(0x16, 0x00);  // ADC控制

    // 步骤7: DAC选择和配置
    printf("[Step 7] DAC selection and configuration...\n");
    ret |= es8311_write_reg(0x17, 0x3C);  // DAC选择
    ret |= es8311_write_reg(0x18, 0x00);  // 左DAC不静音
    ret |= es8311_write_reg(0x19, 0x00);  // 右DAC不静音
    ret |= es8311_write_reg(0x1A, 0x00);  // 左DAC音量
    ret |= es8311_write_reg(0x1B, 0x00);  // 右DAC音量

    // 步骤8: DAC输出路径配置（关键！）
    printf("[Step 8] *** DAC Output Path Configuration ***\n");

    // LOUT MUX配置
    ret |= es8311_write_reg(0x10, 0x00);  // LOUT MUX
    ret |= es8311_write_reg(0x25, 0x03);  // LOUT1 MUX选择
    ret |= es8311_write_reg(0x26, 0x00);  // MIXLEFT
    ret |= es8311_write_reg(0x27, 0x0C);  // LOUT MUX增益 +12dB

    // LOUT使能
    ret |= es8311_write_reg(0x12, 0x10);  // LOUT2使能 (bit4=1)
    ret |= es8311_write_reg(0x13, 0x10);  // LOUT1使能 (bit4=1)

    printf("  LOUT1 enabled, LOUT2 enabled\n");

    // 步骤9: DAC电源和音量
    printf("[Step 9] DAC power and volume...\n");
    ret |= es8311_write_reg(0x2B, 0x00);  // DAC电源正常
    ret |= es8311_write_reg(0x2C, 0x00);  // HP放大器正常
    ret |= es8311_write_reg(0x2D, 0x01);  // HP输出接地参考
    ret |= es8311_write_reg(0x2F, 0x2F);  // 左DAC数字音量最大
    ret |= es8311_write_reg(0x30, 0x2F);  // 右DAC数字音量最大

    printf("  DAC power: ON, Volume: MAX\n");

    // 步骤10: 取消静音
    printf("[Step 10] Unmute DAC...\n");
    ret |= es8311_write_reg(0x32, 0x00);  // DAC不静音
    printf("  DAC unmuted\n");

    // 验证关键寄存器
    printf("\n[Verification] Reading key registers...\n");
    uint8_t val;

    es8311_read_reg(0x02, &val);
    printf("  REG02 (Clock):  0x%02X\n", val);

    es8311_read_reg(0x09, &val);
    printf("  REG09 (DAC fmt): 0x%02X (I2S %s-bit)\n", val, (val & 0x0C) ? "16" : "?");

    es8311_read_reg(0x12, &val);
    bool lout2_ok = (val & 0x10) != 0;
    printf("  REG12 (LOUT2):   0x%02X %s\n", val, lout2_ok ? "[EN]" : "[DIS]");

    es8311_read_reg(0x13, &val);
    bool lout1_ok = (val & 0x10) != 0;
    printf("  REG13 (LOUT1):   0x%02X %s\n", val, lout1_ok ? "[EN]" : "[DIS]");

    es8311_read_reg(0x2B, &val);
    printf("  REG2B (DAC PWR): 0x%02X\n", val);

    es8311_read_reg(0x2F, &val);
    printf("  REG2F (DAC Vol): 0x%02X\n", val);

    es8311_read_reg(0x32, &val);
    printf("  REG32 (Mute):    0x%02X %s\n", val, (val == 0) ? "[UNMUTED]" : "[MUTED]");

    printf("\n========================================\n");
    if (lout1_ok && lout2_ok) {
        printf("  *** ES8311 CONFIGURATION COMPLETE ***\n");
        printf("  Playing 1kHz test tone...\n");
    } else {
        printf("  [X] Configuration incomplete\n");
    }
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

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = EXAMPLE_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = 256,  // MCLK = 256 * sample_rate
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
        printf("Sent %d bytes of audio\n", bytes_written);
    } else {
        printf("[ERROR] I2S write failed\n");
    }

    free(buffer);
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311 Audio Test with PLL Config\n");
    printf("  Using ESP32-WROOM working config\n");
    printf("========================================\n");
    printf("\n");

    if (init_i2c() != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }

    printf("[OK] I2C initialized\n");

    if (es8311_init_with_pll() != ESP_OK) {
        printf("[ERROR] ES8311 init failed\n");
        return;
    }

    if (init_i2s() != ESP_OK) {
        printf("[ERROR] I2S init failed\n");
        return;
    }

    printf("[OK] I2S initialized\n");

    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("\n");
    printf("========================================\n");
    printf("  PLAYING 1kHz TEST TONE\n");
    printf("========================================\n");
    printf("\n");
    printf("LISTEN CAREFULLY for the tone!\n");
    printf("\n");

    int count = 0;
    while (1) {
        printf("\n[Play %d] - 3 seconds of 1kHz...\n", ++count);
        play_1khz_tone();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
