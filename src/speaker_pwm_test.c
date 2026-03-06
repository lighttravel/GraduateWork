/**
 * @file speaker_pwm_test.c
 * @brief 使用PWM直接测试扬声器是否工作
 * 这绕过ES8311，直接用ESP32的PWM驱动扬声器
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "PWM_TEST";

// 使用GPIO13 (I2S DIN引脚) 来输出PWM
#define PWM_GPIO          GPIO_NUM_13

// PWM配置
#define PWM_FREQ_HZ       1000     // 1kHz测试音
#define PWM_DUTY          50       // 50%占空比
#define PWM_RESOLUTION    LEDC_TIMER_10_BIT

void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  Speaker PWM Test\n");
    printf("  Directly driving speaker with PWM\n");
    printf("  This bypasses ES8311\n");
    printf("========================================\n");
    printf("\n");

    printf("Connecting speaker to GPIO%d...\n", PWM_GPIO);
    printf("If speaker works, you should hear 1kHz tone\n");
    printf("\n");

    // 配置PWM定时器
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        printf("[ERROR] Failed to config timer\n");
        return;
    }

    // 配置PWM通道
    ledc_channel_config_t channel_conf = {
        .gpio_num = PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = (1 << 10) * PWM_DUTY / 100,  // 50% duty
        .hpoint = 0,
    };

    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        printf("[ERROR] Failed to config channel\n");
        return;
    }

    printf("[OK] PWM configured\n");
    printf("Frequency: %d Hz\n", PWM_FREQ_HZ);
    printf("Duty: %d%%\n", PWM_DUTY);
    printf("GPIO: %d\n", PWM_GPIO);
    printf("\n");

    printf("Playing 1kHz tone...\n");
    printf("DO YOU HEAR THE TONE?\n");
    printf("\n");
    printf("If YES: Speaker is working, problem is ES8311 configuration\n");
    printf("If NO: Speaker is NOT connected or broken\n");
    printf("\n");

    printf("Will test for 10 seconds, then change frequency...\n");
    printf("\n");

    int freq = PWM_FREQ_HZ;
    int count = 0;

    while (1) {
        printf("\n[Test %d] Frequency: %d Hz\n", ++count, freq);

        // 更新频率
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);

        // 等待5秒
        vTaskDelay(pdMS_TO_TICKS(5000));

        // 切换频率
        if (freq == 1000) {
            freq = 500;
            printf("Changing to 500 Hz (lower pitch)...\n");
        } else if (freq == 500) {
            freq = 2000;
            printf("Changing to 2000 Hz (higher pitch)...\n");
        } else {
            freq = 1000;
            printf("Changing back to 1000 Hz...\n");
        }

        printf("Listen for pitch change!\n");
    }
}
