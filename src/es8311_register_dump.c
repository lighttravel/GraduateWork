/**
 * @file es8311_register_dump.c
 * @brief 读取ES8311所有寄存器状态以诊断问题
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "REG_DUMP";

#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define ES8311_ADDR        0x18

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;

static esp_err_t es8311_read_reg(uint8_t reg, uint8_t *data)
{
    return i2c_master_transmit_receive(i2c_dev, &reg, 1, data, 1, pdMS_TO_TICKS(1000));
}

static void dump_es8311_registers(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311 Register Dump\n");
    printf("========================================\n");
    printf("\n");

    // 读取所有关键寄存器
    uint8_t value;
    esp_err_t ret;

    printf("System Control Registers:\n");
    printf("-------------------------\n");

    // REG00 - 复位和控制
    ret = es8311_read_reg(0x00, &value);
    if (ret == ESP_OK) {
        printf("REG00 (Reset/Ctrl):     0x%02X ", value);
        if (value & 0x80) printf("[RESET ACTIVE]");
        else printf("[Normal]");
        printf("\n");
    }

    // REG02-REG06 - 时钟管理
    ret = es8311_read_reg(0x02, &value);
    if (ret == ESP_OK) {
        printf("REG02 (CLK Mgmt):       0x%02X\n", value);
    }

    ret = es8311_read_reg(0x03, &value);
    if (ret == ESP_OK) {
        printf("REG03 (CLK Mgmt):       0x%02X (ADC OSR=%d)\n", value, (value & 0x07));
    }

    ret = es8311_read_reg(0x04, &value);
    if (ret == ESP_OK) {
        printf("REG04 (CLK Mgmt):       0x%02X\n", value);
    }

    ret = es8311_read_reg(0x05, &value);
    if (ret == ESP_OK) {
        printf("REG05 (CLK Mgmt):       0x%02X\n", value);
    }

    ret = es8311_read_reg(0x06, &value);
    if (ret == ESP_OK) {
        printf("REG06 (CLK Mgmt):       0x%02X\n", value);
    }

    printf("\nDAC Interface Registers:\n");
    printf("-------------------------\n");

    // REG09 - DAC数据接口
    ret = es8311_read_reg(0x09, &value);
    if (ret == ESP_OK) {
        printf("REG09 (DAC Format):     0x%02X ", value);
        switch(value & 0x0F) {
            case 0x00: printf("[I2S]"); break;
            case 0x04: printf("[LJ]"); break;
            case 0x08: printf("[RJ-24]"); break;
            case 0x0C: printf("[RJ-16]"); break;
            default: printf("[Unknown]");
        }
        printf("\n");
    }

    // REG17-REG1B - DAC控制
    ret = es8311_read_reg(0x17, &value);
    if (ret == ESP_OK) {
        printf("REG17 (DAC Sel):        0x%02X\n", value);
    }

    ret = es8311_read_reg(0x18, &value);
    if (ret == ESP_OK) {
        printf("REG18 (DAC Mute L):     0x%02X %s\n", value, (value & 0x80) ? "[MUTED]" : "[UNMUTED]");
    }

    ret = es8311_read_reg(0x19, &value);
    if (ret == ESP_OK) {
        printf("REG19 (DAC Mute R):     0x%02X %s\n", value, (value & 0x80) ? "[MUTED]" : "[UNMUTED]");
    }

    ret = es8311_read_reg(0x1A, &value);
    if (ret == ESP_OK) {
        printf("REG1A (DAC Vol L):      0x%02X (%d dB)\n", value, -(value & 0x7F));
    }

    ret = es8311_read_reg(0x1B, &value);
    if (ret == ESP_OK) {
        printf("REG1B (DAC Vol R):      0x%02X (%d dB)\n", value, -(value & 0x7F));
    }

    printf("\nDAC Power and Volume:\n");
    printf("-------------------------\n");

    // REG2B - DAC电源
    ret = es8311_read_reg(0x2B, &value);
    if (ret == ESP_OK) {
        printf("REG2B (DAC PWR):        0x%02X ", value);
        if (value & 0x01) printf("[PDN L]");
        if (value & 0x02) printf("[PDN R]");
        if (value & 0x04) printf("[PDN HP]");
        if ((value & 0x07) == 0x00) printf("[ALL ON]");
        printf("\n");
    }

    // REG2F - DAC音量
    ret = es8311_read_reg(0x2F, &value);
    if (ret == ESP_OK) {
        printf("REG2F (DAC DVol L):     0x%02X", value);
        if (value & 0x80) printf(" [ZC]");
        printf("\n");
    }

    ret = es8311_read_reg(0x30, &value);
    if (ret == ESP_OK) {
        printf("REG30 (DAC DVol R):     0x%02X", value);
        if (value & 0x80) printf(" [ZC]");
        printf("\n");
    }

    // REG32 - DAC静音
    ret = es8311_read_reg(0x32, &value);
    if (ret == ESP_OK) {
        printf("REG32 (DAC Mute):       0x%02X %s\n", value, (value & 0x03) ? "[MUTED]" : "[UNMUTED]");
    }

    printf("\nDAC Output Path:\n");
    printf("-------------------------\n");

    // REG10, REG12, REG13 - LOUT配置
    ret = es8311_read_reg(0x10, &value);
    if (ret == ESP_OK) {
        printf("REG10 (LOUT MUX):      0x%02X\n", value);
    }

    ret = es8311_read_reg(0x12, &value);
    if (ret == ESP_OK) {
        printf("REG12 (LOUT2):         0x%02X", value);
        if (value & 0x10) printf(" [EN]");
        printf("\n");
    }

    ret = es8311_read_reg(0x13, &value);
    if (ret == ESP_OK) {
        printf("REG13 (LOUT1):         0x%02X", value);
        if (value & 0x10) printf(" [EN]");
        printf("\n");
    }

    ret = es8311_read_reg(0x25, &value);
    if (ret == ESP_OK) {
        printf("REG25 (LOUT1 MUX):     0x%02X\n", value);
    }

    ret = es8311_read_reg(0x27, &value);
    if (ret == ESP_OK) {
        printf("REG27 (LOUT Gain):     0x%02X", value);
        int gain = ((value & 0x0F) * 15) / 10;
        printf(" (%+d dB)\n", gain - 6);
    }

    printf("\nADC Interface (for reference):\n");
    printf("-------------------------\n");

    ret = es8311_read_reg(0x0A, &value);
    if (ret == ESP_OK) {
        printf("REG0A (ADC Format):     0x%02X\n", value);
    }

    ret = es8311_read_reg(0x14, &value);
    if (ret == ESP_OK) {
        printf("REG14 (ADC PGA):       0x%02X\n", value);
    }

    ret = es8311_read_reg(0x16, &value);
    if (ret == ESP_OK) {
        printf("REG16 (ADC Power):     0x%02X", value);
        if (value & 0x01) printf(" [ADC ON]");
        printf("\n");
    }

    printf("\n========================================\n");
    printf("Diagnostic Analysis:\n");
    printf("========================================\n");
    printf("\n");

    // 分析关键状态
    bool dac_on = false;
    bool dac_unmuted = false;
    bool dac_path_enabled = false;

    // 检查DAC电源
    ret = es8311_read_reg(0x2B, &value);
    if (ret == ESP_OK && (value & 0x07) == 0x00) {
        printf("[OK] DAC power is ON\n");
        dac_on = true;
    } else {
        printf("[X] DAC power may be OFF (REG2B=0x%02X)\n", value);
    }

    // 检查静音状态
    ret = es8311_read_reg(0x32, &value);
    if (ret == ESP_OK && (value & 0x03) == 0x00) {
        printf("[OK] DAC is UNMUTED\n");
        dac_unmuted = true;
    } else {
        printf("[X] DAC may be MUTED (REG32=0x%02X)\n", value);
    }

    // 检查DAC音量
    ret = es8311_read_reg(0x2F, &value);
    if (ret == ESP_OK) {
        int vol = value & 0x3F;
        if (vol > 0) {
            printf("[OK] DAC volume is set (0x%02X)\n", value);
        } else {
            printf("[X] DAC volume is at minimum!\n");
        }
    }

    // 检查输出路径
    ret = es8311_read_reg(0x12, &value);
    uint8_t lout2 = value;
    ret = es8311_read_reg(0x13, &value);
    uint8_t lout1 = value;

    if (lout2 & 0x10 || lout1 & 0x10) {
        printf("[OK] Output path may be enabled\n");
        dac_path_enabled = true;
    } else {
        printf("[X] Output path may be DISABLED (LOUT1/2 not enabled)\n");
    }

    printf("\n========================================\n");
    printf("Overall Assessment:\n");
    printf("========================================\n");
    printf("\n");

    if (dac_on && dac_unmuted && dac_path_enabled) {
        printf("ES8311 configuration appears CORRECT.\n");
        printf("\nIf still no sound:\n");
        printf("1. Speaker MUST be connected to ES8311 module SPK terminals\n");
        printf("2. Module MUST have 5V power supply\n");
        printf("3. Check if NS4150B amp needs specific enable signal\n");
    } else {
        printf("ES8311 configuration has ISSUES:\n");
        if (!dac_on) printf("- DAC power not enabled\n");
        if (!dac_unmuted) printf("- DAC is muted\n");
        if (!dac_path_enabled) printf("- Output path not enabled\n");
    }

    printf("\n");
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

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311 Register Analysis Tool\n");
    printf("========================================\n");
    printf("\n");

    if (init_i2c() != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }

    printf("[OK] I2C initialized\n");

    // 首次读取
    dump_es8311_registers();

    printf("\nWaiting 10 seconds before next read...\n");
    vTaskDelay(pdMS_TO_TICKS(10000));

    // 循环读取
    int count = 1;
    while (1) {
        printf("\n========================================\n");
        printf("Scan #%d\n", count++);
        dump_es8311_registers();
        printf("\nNext scan in 15 seconds...\n");
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}
