/**
 * @file es8311_diagnostic.c
 * @brief 诊断ES8311模块 - 简化版本，输出清晰
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "DIAG";

#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define ES8311_ADDR        0x18

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;

static esp_err_t es8311_read_reg(uint8_t reg, uint8_t *data)
{
    return i2c_master_transmit_receive(i2c_dev, &reg, 1, data, 1, pdMS_TO_TICKS(1000));
}

static esp_err_t es8311_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_transmit(i2c_dev, buf, 2, pdMS_TO_TICKS(1000));
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("\n\n");
    printf("****************************************\n");
    printf("*     ES8311 Module Diagnostic Tool    *\n");
    printf("****************************************\n\n");

    // I2C初始化
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    if (i2c_new_master_bus(&bus_cfg, &i2c_bus) != ESP_OK) {
        printf("[ERROR] I2C init failed\n");
        return;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8311_ADDR,
        .scl_speed_hz = 100000,
    };

    if (i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev) != ESP_OK) {
        printf("[ERROR] I2C device add failed\n");
        return;
    }

    printf("[OK] I2C communication working\n\n");

    // 芯片ID
    printf("========================================\n");
    printf("CHIP IDENTIFICATION\n");
    printf("========================================\n");
    uint8_t reg_fd, reg_fe, reg_ff;
    es8311_read_reg(0xFD, &reg_fd);
    es8311_read_reg(0xFE, &reg_fe);
    es8311_read_reg(0xFF, &reg_ff);
    printf("Chip ID: 0x%02X (expected: 0x83) %s\n", reg_fd, (reg_fd == 0x83) ? "[OK]" : "[!]");
    printf("Version: 0x%02X (expected: 0x11) %s\n", reg_fe, (reg_fe == 0x11) ? "[OK]" : "[!]");

    // 当前寄存器状态
    printf("\n========================================\n");
    printf("REGISTER STATUS (Before Fix)\n");
    printf("========================================\n");
    uint8_t reg00, reg02, reg09, reg12, reg13, reg2b, reg2f, reg32;
    es8311_read_reg(0x00, &reg00);
    es8311_read_reg(0x02, &reg02);
    es8311_read_reg(0x09, &reg09);
    es8311_read_reg(0x12, &reg12);
    es8311_read_reg(0x13, &reg13);
    es8311_read_reg(0x2B, &reg2b);
    es8311_read_reg(0x2F, &reg2f);
    es8311_read_reg(0x32, &reg32);

    printf("REG00 [Control]: 0x%02X %s\n", reg00, (reg00 == 0x3F) ? "OK" : "?");
    printf("REG02 [Clock]:   0x%02X %s\n", reg02, (reg02 == 0xF0) ? "OK" : "?");
    printf("REG09 [I2S fmt]: 0x%02X %s\n", reg09, (reg09 == 0x0C) ? "OK" : "?");
    printf("REG12 [LOUT2]:   0x%02X %s\n", reg12, (reg12 & 0x10) ? "ENABLED" : "DISABLED!");
    printf("REG13 [LOUT1]:   0x%02X %s\n", reg13, (reg13 & 0x10) ? "ENABLED" : "DISABLED!");
    printf("REG2B [DAC Pwr]: 0x%02X\n", reg2b);
    printf("REG2F [Volume]:  0x%02X\n", reg2f);
    printf("REG32 [Mute]:    0x%02X %s\n", reg32, (reg32 == 0x00) ? "UNMUTED" : "MUTED!");

    // 修复输出路径
    printf("\n========================================\n");
    printf("APPLYING FIX...\n");
    printf("========================================\n");

    es8311_write_reg(0x00, 0x3F);  // Slave mode
    es8311_write_reg(0x02, 0xF0);  // MCLK mode
    es8311_write_reg(0x03, 0x17);  // OSR settings
    es8311_write_reg(0x09, 0x0C);  // I2S 16-bit
    es8311_write_reg(0x12, 0x10);  // Enable LOUT2
    es8311_write_reg(0x13, 0x10);  // Enable LOUT1
    es8311_write_reg(0x2B, 0x00);  // DAC power
    es8311_write_reg(0x2F, 0x2F);  // Max volume
    es8311_write_reg(0x32, 0x00);  // Unmute

    vTaskDelay(pdMS_TO_TICKS(100));

    // 验证修复
    printf("\n========================================\n");
    printf("REGISTER STATUS (After Fix)\n");
    printf("========================================\n");
    es8311_read_reg(0x12, &reg12);
    es8311_read_reg(0x13, &reg13);
    es8311_read_reg(0x32, &reg32);
    printf("REG12 [LOUT2]:   0x%02X %s\n", reg12, (reg12 & 0x10) ? "ENABLED [OK]" : "DISABLED [FAIL]");
    printf("REG13 [LOUT1]:   0x%02X %s\n", reg13, (reg13 & 0x10) ? "ENABLED [OK]" : "DISABLED [FAIL]");
    printf("REG32 [Mute]:    0x%02X %s\n", reg32, (reg32 == 0x00) ? "UNMUTED [OK]" : "MUTED [FAIL]");

    // 结论
    printf("\n========================================\n");
    printf("DIAGNOSIS RESULT\n");
    printf("========================================\n\n");

    if ((reg12 & 0x10) && (reg13 & 0x10) && (reg32 == 0x00)) {
        printf("Software configuration: ALL OK\n\n");
        printf("If you STILL have NO SOUND:\n\n");
        printf("1. CHECK POWER: Measure VDD-GND with multimeter\n");
        printf("   Expected: 4.8V - 5.2V\n");
        printf("   If < 4.5V: Power supply issue!\n\n");
        printf("2. CHECK SPEAKER:\n");
        printf("   - Must connect to module SPK+/SPK- terminals\n");
        printf("   - NOT to ESP32 pins!\n\n");
        printf("3. CHECK WIRING:\n");
        printf("   ESP32 GPIO11 (DOUT) -> Module DIN\n");
        printf("   ESP32 GPIO14 (BCLK) -> Module SCLK\n");
        printf("   ESP32 GPIO12 (WS)   -> Module LRCK\n");
        printf("   ESP32 GPIO6 (MCLK)  -> Module MCLK\n\n");
    } else {
        printf("Software configuration: HAS ISSUES\n");
    }

    printf("****************************************\n");
    printf("*     Diagnostic complete!             *\n");
    printf("****************************************\n\n");

    // 保持运行
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        printf(".");
        fflush(stdout);
    }
}
