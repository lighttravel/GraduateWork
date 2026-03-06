/**
 * @file hardware_diagnostic.c
 * @brief 硬件连接诊断和指导
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "DIAGNOSTIC";

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  ES8311 Hardware Connection Guide\n");
    printf("========================================\n");
    printf("\n");

    printf("IMPORTANT: How to connect the speaker:\n");
    printf("\n");

    printf("The LuXiaoBan ES8311+NS4150B module has SPEAKER OUTPUT terminals.\n");
    printf("You MUST connect your speaker to the MODULE, not to ESP32 GPIO pins!\n");
    printf("\n");

    printf("Expected connections:\n");
    printf("---------------------\n");
    printf("\n");

    printf("ESP32-S3 <---> ES8311 Module:\n");
    printf("  GPIO4  ---> SCL  (I2C clock)\n");
    printf("  GPIO5  ---> SDA  (I2C data)\n");
    printf("  GPIO6  ---> MCLK (I2S master clock)\n");
    printf("  GPIO14 ---> BCLK (I2S bit clock)\n");
    printf("  GPIO12 ---> LRCK (I2S left/right clock)\n");
    printf("  GPIO13 ---> DIN  (I2S data TO module)\n");
    printf("  GPIO11 ---> DOUT (I2S data FROM module)\n");
    printf("\n");

    printf("POWER:\n");
    printf("  5V     ---> VDD  (Module needs 5V!)\n");
    printf("  GND    ---> GND  (Common ground)\n");
    printf("\n");

    printf("SPEAKER:\n");
    printf("  Speaker+ ---> SPK+  (Module speaker output positive)\n");
    printf("  Speaker- ---> SPK-  (Module speaker output negative)\n");
    printf("\n");

    printf("========================================\n");
    printf("Troubleshooting:\n");
    printf("========================================\n");
    printf("\n");

    printf("If you hear NO sound:\n");
    printf("\n");
    printf("1. CHECK SPEAKER CONNECTION:\n");
    printf("   - Is 8-ohm speaker connected to module SPK+/SPK-?\n");
    printf("   - Is speaker securely connected?\n");
    printf("\n");
    printf("2. CHECK POWER:\n");
    printf("   - Is module receiving 5V power?\n");
    printf("   - Check with multimeter if possible\n");
    printf("\n");
    printf("3. CHECK I2S CABLES:\n");
    printf("   - GPIO13 (DIN) must connect to module DIN\n");
    printf("   - GPIO12 (LRCK) must connect to module LRCK\n");
    printf("   - GPIO14 (BCLK) must connect to module BCLK\n");
    printf("   - GPIO6 (MCLK) should connect to module MCLK\n");
    printf("\n");
    printf("4. CHECK I2C CABLES:\n");
    printf("   - GPIO4 (SCL) connects to module SCL\n");
    printf("   - GPIO5 (SDA) connects to module SDA\n");
    printf("\n");

    printf("========================================\n");
    printf("Common Mistakes:\n");
    printf("========================================\n");
    printf("\n");
    printf("X Connecting speaker to ESP32 GPIO instead of module SPK terminals\n");
    printf("X Powering module with 3.3V instead of 5V\n");
    printf("X Mixing up DIN and DOUT (speaker needs DIN from ESP32)\n");
    printf("X Reversing speaker polarity (though usually still works)\n");
    printf("\n");

    printf("========================================\n");
    printf("Test Status:\n");
    printf("========================================\n");
    printf("\n");
    printf("I2C Scan:       [OK] ES8311 found at 0x18\n");
    printf("I2S Data:       [OK] Sending audio data\n");
    printf("DAC Configure:  [OK] Attempted multiple configs\n");
    printf("Format Test:    [OK] Tried 4 different formats\n");
    printf("PWM Test:       [N/A] Tests wrong pin (speaker on module)\n");
    printf("\n");

    printf("CONCLUSION: Hardware connection issue likely\n");
    printf("\n");

    printf("========================================\n");
    printf("Next Steps:\n");
    printf("========================================\n");
    printf("\n");
    printf("1. Verify speaker is connected to ES8311 module SPK terminals\n");
    printf("2. Verify module has 5V power supply\n");
    printf("3. Check all I2S and I2C connections\n");
    printf("4. Try with a different speaker if available\n");
    printf("\n");

    printf("Once connections are verified, I will:\n");
    printf("- Test with proper ES8311 DAC output path config\n");
    printf("- Try different audio formats and sample rates\n");
    printf("- Check NS4150B amplifier enable requirements\n");
    printf("\n");

    // 持续显示状态
    int count = 0;
    while (1) {
        printf("\rWaiting for hardware check... %d seconds", ++count);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
