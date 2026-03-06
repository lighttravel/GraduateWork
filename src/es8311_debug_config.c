/**
 * @file es8311_debug_config.c
 * @brief ES8311调试配置 - 尝试多种DAC输出路径组合
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "ES8311_DEBUG";

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

// 功放使能引脚（可能需要）
#define PA_EN_PIN         GPIO_NUM_13  // 根据路小班模块，可能需要这个
#define SD_MODE_PIN       GPIO_NUM_12  // 麦克风/耳机选择

static i2s_chan_handle_t tx_handle = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;

// ES8311寄存器地址
#define ES8311_REG00       0x00
#define ES8311_REG02       0x02
#define ES8311_REG03       0x03
#define ES8311_REG04       0x04
#define ES8311_REG05       0x05
#define ES8311_REG06       0x06
#define ES8311_REG09       0x09
#define ES8311_REG2B       0x2B
#define ES8311_REG2F       0x2F
#define ES8311_REG32       0x32
#define ES8311_ADDR        0x18

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
                ESP_LOGI(TAG, "✅ [0x%02X] = 0x%02X", reg, data);
            } else {
                ESP_LOGW(TAG, "⚠️  [0x%02X] 写入0x%02X, 读回0x%02X", reg, data, read_val);
            }
        } else {
            ESP_LOGE(TAG, "❌ [0x%02X] 读取失败", reg);
        }
    } else {
        ESP_LOGE(TAG, "❌ [0x%02X] 写入失败: %s", reg, esp_err_to_name(ret));
    }

    return ret;
}

// 测试I2C通信
static bool test_i2c_communication(void)
{
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "测试I2C通信");
    ESP_LOGI(TAG, "========================================");

    uint8_t value;

    // 尝试读取REG00
    esp_err_t ret = es8311_read_reg(ES8311_REG00, &value);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ I2C通信正常! REG00 = 0x%02X", value);
        return true;
    } else {
        ESP_LOGE(TAG, "❌ I2C通信失败! 无法读取ES8311");
        ESP_LOGE(TAG, "请检查:");
        ESP_LOGE(TAG, "  1. I2C接线 (SCL=GPIO4, SDA=GPIO5)");
        ESP_LOGE(TAG, "  2. ES8311供电 (3.3V或5V)");
        ESP_LOGE(TAG, "  3. ES8311I2C地址应为0x18");
        return false;
    }
}

// 极简ES8311配置 - 只配置最基本的DAC输出
static esp_err_t config_es8311_minimal(void)
{
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "极简ES8311配置");
    ESP_LOGI(TAG, "========================================");

    esp_err_t ret = ESP_OK;

    // 1. 软复位
    ESP_LOGI(TAG, "软复位...");
    ret |= es8311_write_reg_verify(ES8311_REG00, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 2. 设置从机模式
    ESP_LOGI(TAG, "设置从机模式...");
    ret |= es8311_write_reg_verify(ES8311_REG00, 0x00);

    // 3. 配置DAC接口为I2S 16-bit
    ESP_LOGI(TAG, "配置DAC接口...");
    ret |= es8311_write_reg_verify(ES8311_REG09, 0x0C);

    // 4. 设置最大音量
    ESP_LOGI(TAG, "设置音量...");
    ret |= es8311_write_reg_verify(ES8311_REG2F, 0x2F);

    // 5. 取消静音
    ESP_LOGI(TAG, "取消静音...");
    ret |= es8311_write_reg_verify(ES8311_REG32, 0x00);

    // 6. 使能DAC电源
    ESP_LOGI(TAG, "使能DAC电源...");
    ret |= es8311_write_reg_verify(ES8311_REG2B, 0x00);

    // 尝试多种DAC输出路径组合
    ESP_LOGI(TAG, "\n尝试DAC输出路径配置...");

    // 方法1: 基于立创开源项目的配置
    ESP_LOGI(TAG, "方法1: 立创配置");
    ret |= es8311_write_reg_verify(0x17, 0x3C);  // DAC SEL
    ret |= es8311_write_reg_verify(0x18, 0x00);  // DAC MUTE
    ret |= es8311_write_reg_verify(0x19, 0x00);  // DAC MUTE
    ret |= es8311_write_reg_verify(0x1A, 0x00);  // DAC VOL
    ret |= es8311_write_reg_verify(0x1B, 0x00);  // DAC VOL

    // 方法2: 基于ES8311数据手册的LOUT配置
    ESP_LOGI(TAG, "方法2: 数据手册LOUT配置");
    ret |= es8311_write_reg_verify(0x10, 0x00);  // LOUT MUX
    ret |= es8311_write_reg_verify(0x12, 0x00);  // LOUT2S
    ret |= es8311_write_reg_verify(0x13, 0x00);  // LOUT1
    ret |= es8311_write_reg_verify(0x25, 0x03);  // LOUT1 MUX
    ret |= es8311_write_reg_verify(0x27, 0x0C);  // LOUT MUX GAIN

    // 方法3: 尝试使能主输出
    ESP_LOGI(TAG, "方法3: 主输出使能");
    ret |= es8311_write_reg_verify(0x04, 0x01);  // 主机模式
    ret |= es8311_write_reg_verify(0x05, 0x00);

    ESP_LOGI(TAG, "\n✅ ES8311配置完成\n");
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

    ESP_LOGI(TAG, "✅ I2C初始化成功\n");
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

    ESP_LOGI(TAG, "✅ I2S初始化成功\n");
    return ESP_OK;
}

// 生成正弦波
static void generate_sine_wave(int16_t *buffer, int samples, int frequency)
{
    const float amplitude = 32000.0f;  // 更大的振幅
    const float sample_period = 1.0f / EXAMPLE_SAMPLE_RATE;

    for (int i = 0; i < samples; i++) {
        float t = i * sample_period;
        buffer[i * 2] = (int16_t)(amplitude * sinf(2.0f * M_PI * frequency * t));
        buffer[i * 2 + 1] = buffer[i * 2];
    }
}

// 播放1kHz测试音
static void play_1khz_tone(void)
{
    ESP_LOGI(TAG, "\n🔊 播放1kHz测试音...");
    ESP_LOGI(TAG, "如果配置正确，应该听到1kHz音调!\n");

    const int duration_ms = 1000;
    const int total_samples = (EXAMPLE_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buffer = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));

    if (!buffer) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }

    generate_sine_wave(buffer, total_samples, 1000);

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
    } else {
        ESP_LOGE(TAG, "❌ I2S写入失败: %s", esp_err_to_name(ret));
    }

    free(buffer);
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311调试配置测试\n");
    printf("  持续播放1kHz，尝试多种配置\n");
    printf("========================================\n");
    printf("\n");

    // 初始化I2C
    if (init_i2c() != ESP_OK) {
        ESP_LOGE(TAG, "I2C初始化失败");
        return;
    }

    // 测试I2C通信
    if (!test_i2c_communication()) {
        ESP_LOGE(TAG, "I2C通信测试失败，请检查硬件连接");
        return;
    }

    // 配置ES8311
    if (config_es8311_minimal() != ESP_OK) {
        ESP_LOGW(TAG, "ES8311配置部分失败，但继续尝试播放");
    }

    // 初始化I2S
    if (init_i2s() != ESP_OK) {
        ESP_LOGE(TAG, "I2S初始化失败");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "开始循环播放1kHz测试音");
    ESP_LOGI(TAG, "========================================\n");

    // 持续播放1kHz
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "\n【播放次数: %d】", ++count);
        play_1khz_tone();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
