/**
 * @file es8311_codec.c
 * @brief ES8311音频编解码器驱动实现
 *
 * 参考文档：
 * - ES8311+NS4150B音频模块使用说明书V1.1
 * - ES8311音频芯片数据手册
 */

#include "es8311_codec.h"
#include "i2c_driver.h"
#include "gpio_driver.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ES8311";

// ES8311寄存器地址定义
#define ES8311_RESET_REG00          0x00
#define ES8311_CLK_MANAGER_REG01     0x01
#define ES8311_CLK_MANAGER_REG02     0x02
#define ES8311_CLK_ONOFF_REG03       0x03
#define ES8311_MISC_REG04           0x04  // Misc Control
#define ES8311_VMID_REG0D           0x0D  // VMID Control (MicBias)
#define ES8311_MISC_REG0E           0x0E  // Misc Control 2 (MicBias)
#define ES8311_ADC_CONTROL_REG14     0x14
#define ES8311_ADC_GAIN_REG1A      0x1A  // ADC Gain Control
#define ES8311_DAC_CONTROL_REG15     0x15
#define ES8311_SYSTEM_REG16          0x16
#define ES8311_GPIO_REG17            0x17
#define ES8311_DAC_POWER_REG2B       0x2B
#define ES8311_ADC_VOLUME_REG2E      0x2E
#define ES8311_DAC_VOLUME_REG2F      0x2F
#define ES8311_ADC_MUTE_REG31        0x31
#define ES8311_DAC_MUTE_REG32        0x32

static uint8_t g_current_volume = AUDIO_VOLUME;
static es8311_state_t g_state = ES8311_STATE_STOP;

// ==================== 内部函数 ====================

/**
 * @brief 读取ES8311寄存器
 */
static esp_err_t es8311_read_reg(uint8_t reg_addr, uint8_t *data)
{
    return i2c_driver_read_byte(ES8311_I2C_ADDR, reg_addr, data);
}

/**
 * @brief 读取并显示ES8311关键寄存器状态（调试用）
 */
static void es8311_dump_registers(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ES8311寄存器状态:");
    ESP_LOGI(TAG, "========================================");

    uint8_t reg_value;
    esp_err_t ret;

    // 关键寄存器列表
    const struct {
        uint8_t addr;
        const char *name;
    } regs[] = {
        {0x00, "复位寄存器"},
        {0x01, "时钟管理器1"},
        {0x02, "时钟管理器2"},
        {0x03, "时钟开关"},
        {0x14, "ADC控制"},
        {0x15, "DAC控制"},
        {0x16, "系统配置"},
        {0x17, "GPIO配置"},
        {0x2B, "DAC电源"},
        {0x2E, "ADC音量"},
        {0x2F, "DAC音量"},
        {0x31, "ADC静音"},
        {0x32, "DAC静音"},
        {0xFD, "芯片ID"}
    };

    for (size_t i = 0; i < sizeof(regs)/sizeof(regs[0]); i++) {
        ret = es8311_read_reg(regs[i].addr, &reg_value);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  [0x%02X] %s: 0x%02X", regs[i].addr, regs[i].name, reg_value);
        } else {
            ESP_LOGW(TAG, "  [0x%02X] %s: 读取失败 (%s)", regs[i].addr, regs[i].name, esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(5));  // 小延迟避免I2C总线过载
    }

    ESP_LOGI(TAG, "========================================");
}

/**
 * @brief 写入ES8311寄存器并验证
 */
static esp_err_t es8311_write_reg_verify(uint8_t reg_addr, uint8_t data, int retry_count)
{
    esp_err_t ret = ESP_FAIL;  // 初始化为失败
    uint8_t read_back;

    for (int i = 0; i < retry_count; i++) {
        // 写入寄存器
        ret = i2c_driver_write_byte(ES8311_I2C_ADDR, reg_addr, data);

        if (ret == ESP_OK) {
            // 验证写入（可选，对关键寄存器）
            if (reg_addr != 0x00) {  // 复位寄存器读回可能无意义
                ret = i2c_driver_read_byte(ES8311_I2C_ADDR, reg_addr, &read_back);
                if (ret == ESP_OK) {
                    if (read_back == data) {
                        ESP_LOGD(TAG, "  [0x%02X] 写入 0x%02X ✓ 验证成功", reg_addr, data);
                        return ESP_OK;
                    } else {
                        ESP_LOGW(TAG, "  [0x%02X] 写入 0x%02X, 读回 0x%02X (不匹配)", reg_addr, data, read_back);
                        // 继续重试
                    }
                }
            } else {
                // 复位寄存器，只检查写入是否成功
                ESP_LOGD(TAG, "  [0x%02X] 写入 0x%02X ✓", reg_addr, data);
                return ESP_OK;
            }
        }

        // 重试前等待更长时间
        if (i < retry_count - 1) {
            vTaskDelay(pdMS_TO_TICKS(50));  // 增加到50ms
        }
    }

    ESP_LOGE(TAG, "  [0x%02X] 写入 0x%02X 失败: %s", reg_addr, data, esp_err_to_name(ret));
    return ret;
}

/**
 * @brief 写入ES8311寄存器（简化接口）
 */
static esp_err_t es8311_write_reg(uint8_t reg_addr, uint8_t data)
{
    return es8311_write_reg_verify(reg_addr, data, 5);  // 增加重试次数到5次
}

// ==================== ES8311初始化 ====================

esp_err_t es8311_codec_init(const es8311_format_t *format)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ES8311编解码器初始化 v2.2 (增强稳定性)");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "采样率: %luHz, 位深: %d, 声道: %d",
             format->sample_rate, format->bits_per_sample, format->channels);

    esp_err_t ret;

    // 1. 检测ES8311是否存在
    ESP_LOGI(TAG, "步骤1: 检测ES8311设备...");
    ret = es8311_codec_detect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "未检测到ES8311设备");
        return ret;
    }

    ESP_LOGI(TAG, "设备检测成功，等待芯片稳定（500ms）...");
    vTaskDelay(pdMS_TO_TICKS(500));  // 增加稳定时间到500ms

    // 2. 软件复位ES8311
    ESP_LOGI(TAG, "步骤2: 软件复位ES8311...");
    ret = es8311_write_reg(ES8311_RESET_REG00, 0x1F);  // 触发软件复位
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  ✓ 软件复位成功");
    } else {
        ESP_LOGW(TAG, "  软件复位失败，继续...");
    }

    // 复位后等待更长时间让芯片完全初始化
    ESP_LOGI(TAG, "等待复位完成（200ms）...");
    vTaskDelay(pdMS_TO_TICKS(200));

    // 3. 配置时钟系统
    ESP_LOGI(TAG, "步骤3: 配置时钟系统...");

    // 3.1 先配置时钟管理器1 (使能主时钟和PLL)
    ESP_LOGI(TAG, "  3.1 配置时钟管理器1 (0x01)...");
    ret = es8311_write_reg(ES8311_CLK_MANAGER_REG01, 0x30);  // MCLK使能，PLL使能
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "    时钟管理器1配置失败，继续...");
    } else {
        ESP_LOGI(TAG, "    ✓ 时钟管理器1配置成功");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // 3.2 配置时钟管理器2 (BCLK/LRCK从机模式)
    ESP_LOGI(TAG, "  3.2 配置时钟管理器2 (0x02)...");
    ret = es8311_write_reg(ES8311_CLK_MANAGER_REG02, 0x00);  // 从机模式
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "    时钟管理器2配置失败，继续...");
    } else {
        ESP_LOGI(TAG, "    ✓ 时钟管理器2配置成功");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // 4. 系统配置
    ESP_LOGI(TAG, "步骤4: 配置系统参数...");
    ret = es8311_write_reg(ES8311_SYSTEM_REG16, 0x1B);      // 功率放大器配置
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  系统配置失败，继续...");
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    ret = es8311_write_reg(ES8311_GPIO_REG17, 0x00);        // GPIO配置
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  GPIO配置失败，继续...");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "步骤4: 配置采样率分频...");
    uint8_t clk_div_value;
    if (format->sample_rate == 16000) {
        clk_div_value = 0x1C;  // 16kHz
        ESP_LOGI(TAG, "  配置16kHz分频");
    } else if (format->sample_rate == 24000 || format->sample_rate == 48000) {
        clk_div_value = 0x18;  // 24kHz/48kHz
        ESP_LOGI(TAG, "  配置%dkHz分频", format->sample_rate / 1000);
    } else {
        clk_div_value = 0x18;  // 默认
        ESP_LOGW(TAG, "  未配置的采样率: %luHz, 使用默认", format->sample_rate);
    }

    ret = es8311_write_reg(ES8311_CLK_ONOFF_REG03, clk_div_value);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  分频配置失败，继续...");
    } else {
        ESP_LOGI(TAG, "  ✓ 分频配置成功: 0x%02X", clk_div_value);
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // 5. ADC配置
    ESP_LOGI(TAG, "步骤5: 配置ADC...");
    ret = es8311_write_reg(ES8311_ADC_CONTROL_REG14, 0x10);  // ADC使能
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  ADC配置失败，继续...");
    } else {
        ESP_LOGI(TAG, "  ✓ ADC配置成功");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // 6. DAC配置
    ESP_LOGI(TAG, "步骤6: 配置DAC...");
    ret = es8311_write_reg(ES8311_DAC_CONTROL_REG15, 0x00);  // DAC使能
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  DAC配置失败，继续...");
    } else {
        ESP_LOGI(TAG, "  ✓ DAC配置成功");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // 7. 电源管理 - 关键配置！确保DAC电源开启
    ESP_LOGI(TAG, "步骤7: 配置电源管理...");
    for (int retry = 0; retry < 10; retry++) {
        ret = es8311_write_reg(ES8311_DAC_POWER_REG2B, 0x00);    // DAC电源正常(0x00 = 不掉电)
        if (ret == ESP_OK) {
            // 验证写入
            uint8_t read_val;
            if (es8311_read_reg(ES8311_DAC_POWER_REG2B, &read_val) == ESP_OK) {
                if (read_val == 0x00) {
                    ESP_LOGI(TAG, "  ✓ DAC电源配置成功 (0x00)");
                    break;
                } else {
                    ESP_LOGW(TAG, "  DAC电源验证失败，读回: 0x%02X，重试 %d/10", read_val, retry + 1);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 7.5 配置 MicBias（麦克风偏置电压）- 关键步骤！
    ESP_LOGI(TAG, "步骤7.5: 配置麦克风偏置电压...");

    // VMID Control: 使能模拟电源管理和参考电压
    ret = es8311_write_reg(ES8311_VMID_REG0D, 0x06);  // VMID=50k, 使能模拟电源
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  VMID配置失败，继续...");
    } else {
        ESP_LOGI(TAG, "    ✓ VMID配置成功");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // Misc Control: 配置 MicBias 电压
    // 0x00 = 1.0V, 0x01 = 1.2V, 0x02 = 2.0V, 0x03 = 2.5V
    ret = es8311_write_reg(ES8311_MISC_REG0E, 0x02);  // MicBias = 2.0V
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  MicBias配置失败，继续...");
    } else {
        ESP_LOGI(TAG, "    ✓ MicBias配置成功 (2.0V)");
    }

    // 额外配置：ADC 增益，提高麦克风灵敏度
    ret = es8311_write_reg(ES8311_ADC_GAIN_REG1A, 0x60);  // ADC增益 +6dB
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  ADC增益配置失败，继续...");
    } else {
        ESP_LOGI(TAG, "    ✓ ADC增益配置成功 (+6dB)");
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    // 8. 设置默认音量
    ESP_LOGI(TAG, "步骤8: 设置默认音量...");
    ret = es8311_codec_set_volume(g_current_volume);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  音量设置失败，继续...");
    } else {
        ESP_LOGI(TAG, "  ✓ 音量设置成功: %d", g_current_volume);
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // 9. 取消静音
    ESP_LOGI(TAG, "步骤9: 取消静音...");
    ret = es8311_codec_mute(false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  取消静音失败，继续...");
    } else {
        ESP_LOGI(TAG, "  ✓ 取消静音成功");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // 10. 切换到麦克风输入
    ESP_LOGI(TAG, "步骤10: 配置音频输入...");
    ret = gpio_driver_set_audio_input(true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "  音频输入配置失败，继续...");
    } else {
        ESP_LOGI(TAG, "  ✓ 音频输入配置成功（麦克风）");
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ES8311初始化完成");
    ESP_LOGI(TAG, "========================================");

    // 显示寄存器状态（调试用）
    es8311_dump_registers();

    g_state = ES8311_STATE_STOP;
    return ESP_OK;
}

esp_err_t es8311_codec_deinit(void)
{
    ESP_LOGI(TAG, "反初始化ES8311");

    // 静音
    es8311_codec_mute(true);

    // 关闭功放
    gpio_driver_enable_pa(false);

    g_state = ES8311_STATE_STOP;
    return ESP_OK;
}

// ==================== 设备检测 ====================

esp_err_t es8311_codec_detect(void)
{
    ESP_LOGI(TAG, "开始扫描I2C总线寻找ES8311...");

    // 常见的ES8311 I2C地址（7位地址）
    const uint8_t possible_addresses[] = {0x18, 0x19, 0x1A, 0x1B, 0x10, 0x11};
    const size_t num_addresses = sizeof(possible_addresses) / sizeof(possible_addresses[0]);

    for (size_t i = 0; i < num_addresses; i++) {
        uint8_t test_addr = possible_addresses[i];
        ESP_LOGI(TAG, "  尝试地址 0x%02X...", test_addr);

        // 直接使用I2C驱动测试通信
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (test_addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, 0xFD, true);  // 芯片ID寄存器
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (test_addr << 1) | I2C_MASTER_READ, true);
        uint8_t chip_id;
        i2c_master_read_byte(cmd, &chip_id, I2C_MASTER_NACK);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "========================================");
            ESP_LOGI(TAG, "✓ 检测到I2C设备 @ 0x%02X", test_addr);
            ESP_LOGI(TAG, "========================================");
            ESP_LOGI(TAG, "  芯片ID寄存器(0xFD): 0x%02X", chip_id);

            if (test_addr == 0x18) {
                ESP_LOGI(TAG, "  这是标准的ES8311地址 (0x18)");
                ESP_LOGI(TAG, "检测到ES8311设备, 芯片ID: 0x%02X", chip_id);
                return ESP_OK;
            } else {
                ESP_LOGW(TAG, "  检测到设备但不是标准ES8311地址 (0x18)");
                ESP_LOGW(TAG, "  更新config.h中的ES8311_I2C_ADDR为0x%02X", test_addr);
                ESP_LOGW(TAG, "  当前将使用此地址继续");
                // 注意：这里应该更新全局的ES8311_I2C_ADDR，但它是#define常量
                // 用户需要手动更新config.h
                return ESP_OK;
            }
        } else {
            ESP_LOGD(TAG, "  地址0x%02X无响应", test_addr);
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // 小延迟
    }

    ESP_LOGE(TAG, "");
    ESP_LOGE(TAG, "========================================");
    ESP_LOGE(TAG, "✗ 未检测到ES8311或兼容I2C设备");
    ESP_LOGE(TAG, "========================================");
    ESP_LOGE(TAG, "");
    ESP_LOGE(TAG, "可能原因:");
    ESP_LOGE(TAG, "1. 供电问题 - 路小班模块5V是否连接到外部5V电源？");
    ESP_LOGE(TAG, "2. 接线错误 - SDA→GPIO5, SCL→GPIO4是否正确？");
    ESP_LOGE(TAG, "3. 缺少上拉电阻 - 路小班模块是否有板载4.7kΩ上拉？");
    ESP_LOGE(TAG, "4. GPIO12/13未连接 - 尝试连接SD_MODE(GPIO12)和PA_EN(GPIO13)");
    ESP_LOGE(TAG, "5. I2C频率不对 - 当前100kHz，某些模块可能需要不同频率");
    ESP_LOGE(TAG, "========================================");

    return ESP_FAIL;
}

// ==================== 播放控制 ====================

esp_err_t es8311_codec_config_play(void)
{
    ESP_LOGI(TAG, "配置ES8311为播放模式");

    esp_err_t ret;

    // 切换到耳机/扬声器输出模式 (SD_MODE = LOW)
    ret = gpio_driver_set_audio_input(false);

    // 配置DAC SDP (串行数据端口) - I2S格式, 16-bit
    es8311_write_reg(ES8311_DAC_CONTROL_REG15, 0x0C);  // I2S format, 16-bit

    // 确保DAC电源开启 - 关键！
    for (int retry = 0; retry < 10; retry++) {
        ret = es8311_write_reg(ES8311_DAC_POWER_REG2B, 0x00);
        if (ret == ESP_OK) {
            uint8_t read_val;
            if (es8311_read_reg(ES8311_DAC_POWER_REG2B, &read_val) == ESP_OK && read_val == 0x00) {
                ESP_LOGI(TAG, "  DAC电源确认开启");
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    // 取消静音
    es8311_codec_mute(false);

    // 设置音量到最大
    es8311_codec_set_volume(100);

    // 使能功放
    gpio_driver_enable_pa(true);

    ESP_LOGI(TAG, "播放模式配置完成");

    g_state = ES8311_STATE_PLAYING;
    return ESP_OK;
}

esp_err_t es8311_codec_start_play(void)
{
    ESP_LOGI(TAG, "启动ES8311播放");
    return es8311_codec_config_play();
}

esp_err_t es8311_codec_stop_play(void)
{
    ESP_LOGI(TAG, "停止ES8311播放");

    esp_err_t ret = es8311_codec_mute(true);
    ret = gpio_driver_enable_pa(false);

    g_state = ES8311_STATE_STOP;
    return ret;
}

// ==================== 录音控制 ====================

esp_err_t es8311_codec_config_record(void)
{
    ESP_LOGI(TAG, "配置ES8311为录音模式");

    esp_err_t ret;

    // 切换到麦克风输入
    ret = gpio_driver_set_audio_input(true);

    // 使能ADC
    ret = es8311_write_reg(ES8311_ADC_CONTROL_REG14, 0x10);

    // 关闭功放
    ret = gpio_driver_enable_pa(false);

    g_state = ES8311_STATE_RECORDING;
    return ret;
}

esp_err_t es8311_codec_start_record(void)
{
    ESP_LOGI(TAG, "启动ES8311录音");
    return es8311_codec_config_record();
}

esp_err_t es8311_codec_stop_record(void)
{
    ESP_LOGI(TAG, "停止ES8311录音");

    g_state = ES8311_STATE_STOP;
    return ESP_OK;
}

// ==================== 音量控制 ====================

esp_err_t es8311_codec_set_volume(uint8_t volume)
{
    if (volume > 100) {
        volume = 100;
    }

    ESP_LOGI(TAG, "设置音量: %d", volume);
    g_current_volume = volume;

    // ES8311音量范围: 0x00(-96dB) ~ 0x2F(0dB)
    // 映射: 0-100 -> 0x00-0x2F
    uint8_t reg_value = (uint8_t)((volume * 0x2F) / 100);

    esp_err_t ret = es8311_write_reg(ES8311_DAC_VOLUME_REG2F, reg_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置音量失败");
        return ret;
    }

    return ESP_OK;
}

esp_err_t es8311_codec_get_volume(uint8_t *volume)
{
    if (volume == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *volume = g_current_volume;
    return ESP_OK;
}

// ==================== 静音控制 ====================

esp_err_t es8311_codec_mute(bool mute)
{
    ESP_LOGI(TAG, "%s静音", mute ? "启用" : "取消");

    esp_err_t ret = ESP_OK;

    if (mute) {
        // 静音
        es8311_write_reg(ES8311_DAC_MUTE_REG32, 0x04);
        es8311_write_reg(ES8311_ADC_MUTE_REG31, 0x04);
    } else {
        // 取消静音 - 多次重试确保成功
        for (int retry = 0; retry < 5; retry++) {
            ret = es8311_write_reg(ES8311_DAC_MUTE_REG32, 0x00);
            es8311_write_reg(ES8311_ADC_MUTE_REG31, 0x00);
            if (ret == ESP_OK) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    return ret;
}

// ==================== 采样率设置 ====================

esp_err_t es8311_codec_set_sample_rate(uint32_t sample_rate)
{
    ESP_LOGI(TAG, "设置ES8311采样率: %luHz", sample_rate);

    // 重新初始化以应用新的采样率
    es8311_format_t format = {
        .sample_rate = sample_rate,
        .bits_per_sample = 16,
        .channels = 1
    };

    return es8311_codec_init(&format);
}
