/**
 * @file test_audio_simple.c
 * @brief 简单音频测试 - 只测试 I2S → ES8311 → 喇叭
 *
 * 功能：
 * - 初始化 I2C 和 I2S
 * - 配置 ES8311 编解码器
 * - 生成 1kHz 正弦波 PCM (16kHz/16bit/mono)
 * - 连续播放 2 秒
 *
 * 编译：pio run
 * 上传：pio run --target upload
 * 监控：pio device monitor
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "AUDIO_TEST";

// ==================== 引脚配置 (路小班ES8311+NS4150B模块) ====================
#define I2C_SCL_PIN        GPIO_NUM_8     // I2C时钟 ✅
#define I2C_SDA_PIN        GPIO_NUM_9     // I2C数据 ✅
#define I2S_BCLK_PIN       GPIO_NUM_15    // I2S位时钟 ✅
#define I2S_LRCK_PIN       GPIO_NUM_16    // I2S字选择 ✅
#define I2S_DIN_PIN        GPIO_NUM_17    // ESP32 → ES8311 (播放) ✅
#define I2S_DOUT_PIN       GPIO_NUM_18    // ES8311 → ESP32 (录音) ✅
#define I2S_MCLK_PIN       GPIO_NUM_38    // I2S主时钟 ✅ (避开UART0冲突)
#define PA_EN_PIN          GPIO_NUM_11    // 路小班模块无此引脚(未使用)

#define ES8311_I2C_ADDR    0x18

// ==================== 全局句柄 ====================
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;
static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;

// ==================== ES8311 寄存器定义 ====================
#define ES8311_RESET_REG00          0x00
#define ES8311_CLK_MANAGER_REG01    0x01
#define ES8311_CLK_MANAGER_REG02    0x02
#define ES8311_CLK_ONOFF_REG03      0x03
#define ES8311_MISC_REG04           0x04
#define ES8311_SDPIN_REG09          0x09    // DAC 数据接口
#define ES8311_SDPOUT_REG0A         0x0A    // ADC 数据接口
#define ES8311_VMID_REG0D           0x0D
#define ES8311_MISC_REG0E           0x0E
#define ES8311_SYSTEM_REG12         0x12
#define ES8311_ADC_CONTROL_REG14    0x14
#define ES8311_ADC_REG15            0x15
#define ES8311_GPIO_REG17           0x17
#define ES8311_DAC_POWER_REG2B      0x2B
#define ES8311_DAC_VOLUME_REG2F     0x2F
#define ES8311_DAC_MUTE_REG32       0x32
#define ES8311_DAC_REG37            0x37
#define ES8311_GPIO_REG44           0x44
#define ES8311_GP_REG45             0x45

// ==================== I2C 函数 ====================
static esp_err_t es8311_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    esp_err_t ret = i2c_master_transmit(i2c_dev, buf, 2, pdMS_TO_TICKS(1000));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "I2C 写入 [0x%02X] = 0x%02X OK", reg, data);
    } else {
        ESP_LOGE(TAG, "I2C 写入 [0x%02X] 失败: %s", reg, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t es8311_read_reg(uint8_t reg, uint8_t *data)
{
    return i2c_master_transmit_receive(i2c_dev, &reg, 1, data, 1, pdMS_TO_TICKS(1000));
}

// ==================== 初始化函数 ====================

static esp_err_t init_i2c(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "初始化 I2C");
    ESP_LOGI(TAG, "========================================");

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = true,
        },
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 I2C 总线失败: %s", esp_err_to_name(ret));
        return ret;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8311_I2C_ADDR,
        .scl_speed_hz = 100000,
    };

    ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "添加 I2C 设备失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C 初始化成功");
    return ESP_OK;
}

static esp_err_t init_i2s(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "初始化 I2S");
    ESP_LOGI(TAG, "========================================");

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 I2S 通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = 16000,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_128,  // 不使用 MCLK
#ifdef I2S_HW_VERSION_2
            .ext_clk_freq_hz = 0,
#endif
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
#ifdef I2S_HW_VERSION_2
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false
#endif
        },
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC,  // ESP32不支持GPIO14输出MCLK，不使用
            .bclk = I2S_BCLK_PIN,
            .ws = I2S_LRCK_PIN,
            .dout = I2S_DIN_PIN,
            .din = I2S_DOUT_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化 I2S TX 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_init_std_mode(rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化 I2S RX 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2S 初始化成功");
    return ESP_OK;
}

static esp_err_t init_es8311(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "配置 ES8311 寄存器 (无MCLK模式)");
    ESP_LOGI(TAG, "========================================");

    esp_err_t ret = ESP_OK;

    // 1. 增强 I2C 抗干扰
    ret |= es8311_write_reg(0x44, 0x08);
    ret |= es8311_write_reg(0x44, 0x08);

    // 2. 配置时钟系统 - 使用内部PLL从BCLK生成时钟 (无MCLK)
    // REG01: bit7=1(PLL使能), bit6=0(MCLK禁用), bit5=1(ADC CLK), bit4=1(DAC CLK)
    ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG01, 0x9F);  // PLL使能, MCLK禁用
    ESP_LOGI(TAG, ">>> 时钟配置: PLL使能, 无外部MCLK");

    // REG02: 从机模式, BCLK作为PLL参考
    ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG02, 0x00);

    // REG03: ADC OSR
    ret |= es8311_write_reg(ES8311_CLK_ONOFF_REG03, 0x10);

    // REG04: DAC OSR
    ret |= es8311_write_reg(ES8311_MISC_REG04, 0x10);

    // REG05: CLK_DIVIDER
    ret |= es8311_write_reg(0x05, 0x00);

    // 3. 系统配置
    ret |= es8311_write_reg(0x0B, 0x00);
    ret |= es8311_write_reg(0x0C, 0x00);
    ret |= es8311_write_reg(0x10, 0x1F);
    ret |= es8311_write_reg(0x11, 0x7F);

    // 4. 软件复位
    ret |= es8311_write_reg(ES8311_RESET_REG00, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 5. 设置从机模式
    uint8_t regv = 0;
    es8311_read_reg(ES8311_RESET_REG00, &regv);
    regv &= 0xBF;  // Slave mode
    ret |= es8311_write_reg(ES8311_RESET_REG00, regv);

    // 6. 使能所有时钟 (PLL模式)
    ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG01, 0x9F);

    // 7. ADC 配置
    ret |= es8311_write_reg(0x16, 0x24);
    ret |= es8311_write_reg(0x13, 0x10);
    ret |= es8311_write_reg(0x1B, 0x0A);
    ret |= es8311_write_reg(0x1C, 0x6A);

    // 8. DAC/ADC 数据接口配置 (16-bit I2S)
    ret |= es8311_write_reg(ES8311_SDPIN_REG09, 0x0C);
    ret |= es8311_write_reg(ES8311_SDPOUT_REG0A, 0x0C);

    // 9. VMID 和 MicBias 配置
    ret |= es8311_write_reg(ES8311_VMID_REG0D, 0x01);
    ret |= es8311_write_reg(ES8311_MISC_REG0E, 0x02);

    // 10. 设置音量 (最大)
    ret |= es8311_write_reg(ES8311_DAC_VOLUME_REG2F, 0x2F);

    ESP_LOGI(TAG, "ES8311 基础配置完成 (PLL模式)");
    return ret;
}

static esp_err_t es8311_start_dac(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ES8311 启动 DAC");
    ESP_LOGI(TAG, "========================================");

    esp_err_t ret = ESP_OK;

    // 启动序列 (参考 ESP-ADF)
    ret |= es8311_write_reg(ES8311_GPIO_REG17, 0xBF);
    ret |= es8311_write_reg(ES8311_MISC_REG0E, 0x02);
    ret |= es8311_write_reg(ES8311_SYSTEM_REG12, 0x00);
    ret |= es8311_write_reg(ES8311_ADC_CONTROL_REG14, 0x1A);
    ret |= es8311_write_reg(ES8311_VMID_REG0D, 0x01);
    ret |= es8311_write_reg(ES8311_ADC_REG15, 0x40);

    // DAC 配置
    ret |= es8311_write_reg(ES8311_DAC_REG37, 0x08);

    // DAC 电源开启 (关键!)
    ret |= es8311_write_reg(ES8311_DAC_POWER_REG2B, 0x00);
    ESP_LOGI(TAG, ">>> [0x2B] = 0x00 (DAC 电源开启)");

    // DAC 取消静音 (关键!)
    ret |= es8311_write_reg(ES8311_DAC_MUTE_REG32, 0x00);
    ESP_LOGI(TAG, ">>> [0x32] = 0x00 (DAC 取消静音)");

    // GPIO 配置
    ret |= es8311_write_reg(ES8311_GP_REG45, 0x00);
    ret |= es8311_write_reg(ES8311_GPIO_REG44, 0x58);

    // 启用 DAC 数据接口 (清除 bit 6)
    uint8_t dac_iface = 0;
    es8311_read_reg(ES8311_SDPIN_REG09, &dac_iface);
    dac_iface &= 0xBF;  // 清除 bit 6 启用接口
    ret |= es8311_write_reg(ES8311_SDPIN_REG09, dac_iface);
    ESP_LOGI(TAG, ">>> [0x09] = 0x%02X (DAC 接口启用)", dac_iface);

    ESP_LOGI(TAG, "ES8311 DAC 启动完成");
    return ret;
}

static esp_err_t init_pa_gpio(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "配置 PA GPIO");
    ESP_LOGI(TAG, "========================================");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PA_EN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置 PA GPIO 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 默认关闭
    gpio_set_level(PA_EN_PIN, 0);
    ESP_LOGI(TAG, "PA GPIO 配置成功: pin=%d", PA_EN_PIN);
    return ESP_OK;
}

static void set_pa_enable(bool enable)
{
    gpio_set_level(PA_EN_PIN, enable ? 1 : 0);
    ESP_LOGI(TAG, ">>> PA_EN (GPIO%d) = %d", PA_EN_PIN, enable ? 1 : 0);
}

// ==================== 音频生成和播放 ====================

static void play_sine_wave(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "生成并播放 1kHz 正弦波");
    ESP_LOGI(TAG, "========================================");

    // 参数
    const int sample_rate = 16000;    // 16kHz
    const int freq = 1000;            // 1kHz
    const int duration_sec = 2;       // 2秒
    const int samples = sample_rate * duration_sec;
    const double amplitude = 30000.0; // 接近最大振幅

    ESP_LOGI(TAG, "参数: %dHz, %d秒, %d采样点, 振幅=%.0f",
             freq, duration_sec, samples, amplitude);

    // 分配缓冲区
    int16_t *buffer = (int16_t *)malloc(samples * sizeof(int16_t));
    if (buffer == NULL) {
        ESP_LOGE(TAG, "分配缓冲区失败!");
        return;
    }

    // 生成正弦波
    ESP_LOGI(TAG, "生成正弦波数据...");
    for (int i = 0; i < samples; i++) {
        buffer[i] = (int16_t)(amplitude * sin(2.0 * M_PI * freq * i / sample_rate));
    }

    // 启用 I2S 通道
    ESP_LOGI(TAG, "启用 I2S TX 通道...");
    esp_err_t ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用 I2S 失败: %s", esp_err_to_name(ret));
        free(buffer);
        return;
    }

    // 启用 PA
    ESP_LOGI(TAG, "启用功放...");
    set_pa_enable(true);

    // 播放
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, ">>> 开始播放! 请听喇叭是否有声音 <<<");
    ESP_LOGI(TAG, "========================================");

    size_t bytes_written = 0;
    size_t total_bytes = samples * sizeof(int16_t);

    ret = i2s_channel_write(tx_handle, buffer, total_bytes, &bytes_written, pdMS_TO_TICKS(5000));

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, ">>> 播放完成! 写入 %d 字节 <<<", (int)bytes_written);
    } else {
        ESP_LOGE(TAG, "播放失败: %s", esp_err_to_name(ret));
    }

    // 等待 DMA 完成
    vTaskDelay(pdMS_TO_TICKS(100));

    // 禁用 PA
    set_pa_enable(false);

    // 禁用 I2S
    i2s_channel_disable(tx_handle);

    free(buffer);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "测试完成");
    ESP_LOGI(TAG, "========================================");
}

// ==================== 主程序 ====================

void app_main(void)
{
    printf("\n\n");
    printf("****************************************************\n");
    printf("*        简单音频测试 - I2S → ES8311 → 喇叭        *\n");
    printf("*                                                  *\n");
    printf("*  测试内容: 1kHz 正弦波, 2秒                      *\n");
    printf("*  排除因素: WiFi, TTS, 解码                       *\n");
    printf("****************************************************\n\n");

    // 1. 初始化 I2C
    if (init_i2c() != ESP_OK) {
        ESP_LOGE(TAG, "I2C 初始化失败，程序终止");
        return;
    }

    // 2. 初始化 I2S
    if (init_i2s() != ESP_OK) {
        ESP_LOGE(TAG, "I2S 初始化失败，程序终止");
        return;
    }

    // 3. 初始化 PA GPIO
    if (init_pa_gpio() != ESP_OK) {
        ESP_LOGE(TAG, "PA GPIO 初始化失败，程序终止");
        return;
    }

    // 4. 配置 ES8311
    if (init_es8311() != ESP_OK) {
        ESP_LOGW(TAG, "ES8311 配置有错误，但继续测试");
    }

    // 5. 启动 ES8311 DAC
    if (es8311_start_dac() != ESP_OK) {
        ESP_LOGW(TAG, "ES8311 DAC 启动有错误，但继续测试");
    }

    // 6. 等待 1 秒让系统稳定
    ESP_LOGI(TAG, "等待 1 秒...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 7. 播放正弦波
    play_sine_wave();

    // 8. 循环测试 (每 3 秒播放一次)
    ESP_LOGI(TAG, "每 3 秒重复播放一次，按 Ctrl+C 退出");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        play_sine_wave();
    }
}
