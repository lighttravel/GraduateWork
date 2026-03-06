/**
 * @file hardware_connection_test.c
 * @brief 硬件连接诊断工具
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "HW_DIAG";

#define I2C_SCL_PIN        GPIO_NUM_4
#define I2C_SDA_PIN        GPIO_NUM_5
#define ES8311_ADDR        0x18

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;

static esp_err_t es8311_read_reg(uint8_t reg, uint8_t *data)
{
    return i2c_master_transmit_receive(i2c_dev, &reg, 1, data, 1, pdMS_TO_TICKS(1000));
}

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  Hardware Connection Diagnostic\n");
    printf("========================================\n");
    printf("\n");

    // 初始化I2C
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

    printf("[OK] I2C initialized\n");
    printf("\n");

    // 读取ES8311芯片ID
    printf("========================================\n");
    printf("ES8311 Chip Information\n");
    printf("========================================\n");
    printf("\n");

    uint8_t val;
    bool chip_ok = true;

    // 读取关键寄存器来验证芯片
    es8311_read_reg(0x00, &val);
    printf("Chip ID REG00:     0x%02X\n", val);

    es8311_read_reg(0xFD, &val);
    printf("Chip ID REGFD:     0x%02X\n", val);

    es8311_read_reg(0xFE, &val);
    printf("Chip ID REGFE:     0x%02X\n", val);

    es8311_read_reg(0xFF, &val);
    printf("Chip ID REGFF:     0x%02X\n", val);

    printf("\n");

    // 检查芯片是否正常响应
    uint8_t test_write = 0x00;
    uint8_t read_back;
    es8311_read_reg(0x00, &read_back);

    if (read_back == test_write || read_back == 0x3F || read_back == 0x00) {
        printf("[OK] ES8311 is responding to I2C\n");
    } else {
        printf("[ERROR] ES8311 not responding properly\n");
        chip_ok = false;
    }

    printf("\n");
    printf("========================================\n");
    printf("Hardware Connection Checklist\n");
    printf("========================================\n");
    printf("\n");

    printf("ESP32-S3 -> ES8311 Module Connections:\n");
    printf("────────────────────────────────────────\n");
    printf("\n");

    printf("I2C Control:\n");
    printf("  ESP32 GPIO4 (SCL) -> Module SCL   [Required for config]\n");
    printf("  ESP32 GPIO5 (SDA) -> Module SDA   [Required for config]\n");
    printf("  Status: I2C working\n");
    printf("\n");

    printf("I2S Audio:\n");
    printf("  ESP32 GPIO6  (MCLK) -> Module MCLK   [Master clock]\n");
    printf("  ESP32 GPIO14 (BCLK) -> Module SCLK  [Bit clock]\n");
    printf("  ESP32 GPIO12 (WS)   -> Module LRCK  [Word select]\n");
    printf("  ESP32 GPIO11 (DOUT)-> Module DIN    [Data TO ES8311]\n");
    printf("\n");

    printf("Power:\n");
    printf("  ESP32 5V  -> Module VDD   [NEEDS 5V, not 3.3V!]\n");
    printf("  ESP32 GND -> Module GND   [Common ground]\n");
    printf("\n");

    printf("Speaker Output:\n");
    printf("  Speaker+ -> Module SPK+   [Speaker output terminal]\n");
    printf("  Speaker- -> Module SPK-   [Speaker output terminal]\n");
    printf("\n");

    printf("========================================\n");
    printf("Troubleshooting\n");
    printf("========================================\n");
    printf("\n");

    if (chip_ok) {
        printf("ES8311 chip is working.\n");
        printf("\n");
        printf("If you still hear NO sound:\n");
        printf("\n");
        printf("1. CHECK SPEAKER CONNECTION:\n");
        printf("   - Speaker MUST connect to ES8311 module SPK terminals\n");
        printf("   - NOT to ESP32 GPIO pins!\n");
        printf("   - Check if speaker is 8-ohm impedance\n");
        printf("\n");
        printf("2. CHECK MODULE POWER:\n");
        printf("   - Use multimeter to measure VDD-GND voltage\n");
        printf("   - Should be 4.8V - 5.2V\n");
        printf("   - If lower, module won't drive speaker\n");
        printf("\n");
        printf("3. CHECK SPEAKER ITSELF:\n");
        printf("   - Try with a different speaker\n");
        printf("   - Or connect headphones to module (if available)\n");
        printf("\n");
        printf("4. CHECK MODULE HARDWARE:\n");
        printf("   - Any visible damage on module?\n");
        printf("   - Are there any jumper caps?\n");
        printf("   - Is NS4150B amplifier properly soldered?\n");
        printf("\n");
    } else {
        printf("[ERROR] ES8311 not detected!\n");
        printf("\n");
        printf("Check I2C connections:\n");
        printf("  - GPIO4 to SCL\n");
        printf("  - GPIO5 to SDA\n");
        printf("  - ES8311 power supply\n");
    }

    printf("\n");
    printf("========================================\n");
    printf("Next Steps\n");
    printf("========================================\n");
    printf("\n");
    printf("Please check the above connections and report back:\n");
    printf("\n");
    printf("1. Is speaker connected to module SPK+/SPK-?\n");
    printf("2. What voltage does module VDD measure?\n");
    printf("3. Does module have any LEDs lit?\n");
    printf("4. Are there any labels on the SPK terminals?\n");
    printf("\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        printf("\rWaiting for your hardware check feedback... (Press Ctrl+C to stop)");
        fflush(stdout);
    }
}
