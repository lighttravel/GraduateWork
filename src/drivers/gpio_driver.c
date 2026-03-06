/**
 * @file gpio_driver.c
 * @brief GPIO控制驱动实现
 *
 * 路小班ES8311+NS4150B模块适配:
 * - 模块无SD_MODE和PA_EN控制引脚
 * - 仅LED_STATUS引脚可用
 */

#include "gpio_driver.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "GPIO_DRV";

// ==================== GPIO初始化 ====================

esp_err_t gpio_driver_init(void)
{
    ESP_LOGI(TAG, "初始化GPIO驱动");

    // 配置控制引脚为输出
    // 注意: 路小班模块没有SD_MODE和PA_EN引脚,仅初始化LED
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_STATUS_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 初始状态设置
    gpio_set_level(LED_STATUS_PIN, 0); // LED关闭

    ESP_LOGI(TAG, "GPIO驱动初始化完成 (路小班模块: 无SD_MODE/PA_EN引脚)");
    return ESP_OK;
}

// ==================== GPIO控制 ====================

esp_err_t gpio_driver_set_level(gpio_ctrl_pin_t pin, uint32_t level)
{
    gpio_num_t gpio_num;

    switch (pin) {
        case GPIO_CTRL_SD_MODE:
            ESP_LOGW(TAG, "SD_MODE引脚控制被忽略 (路小班模块无此引脚)");
            return ESP_OK;  // 路小班模块无此引脚,直接返回成功
        case GPIO_CTRL_PA_EN:
            ESP_LOGW(TAG, "PA_EN引脚控制被忽略 (路小班模块无此引脚)");
            return ESP_OK;  // 路小班模块无此引脚,直接返回成功
        case GPIO_CTRL_LED_STATUS:
            gpio_num = LED_STATUS_PIN;
            break;
        default:
            ESP_LOGE(TAG, "未知的GPIO控制引脚: %d", pin);
            return ESP_ERR_INVALID_ARG;
    }

    gpio_set_level(gpio_num, level);
    return ESP_OK;
}

esp_err_t gpio_driver_get_level(gpio_ctrl_pin_t pin, uint32_t *level)
{
    gpio_num_t gpio_num;

    switch (pin) {
        case GPIO_CTRL_SD_MODE:
            ESP_LOGW(TAG, "SD_MODE引脚读取被忽略 (路小班模块无此引脚)");
            *level = 1;  // 返回默认值(麦克风模式)
            return ESP_OK;
        case GPIO_CTRL_PA_EN:
            ESP_LOGW(TAG, "PA_EN引脚读取被忽略 (路小班模块无此引脚)");
            *level = 1;  // 返回默认值(功放开启)
            return ESP_OK;
        case GPIO_CTRL_LED_STATUS:
            gpio_num = LED_STATUS_PIN;
            break;
        default:
            ESP_LOGE(TAG, "未知的GPIO控制引脚: %d", pin);
            return ESP_ERR_INVALID_ARG;
    }

    *level = gpio_get_level(gpio_num);
    return ESP_OK;
}

// ==================== 功放控制 ====================

esp_err_t gpio_driver_enable_pa(bool enable)
{
    ESP_LOGW(TAG, "功放控制被忽略 (路小班模块无PA_EN引脚,内部控制)");
    return ESP_OK;  // 路小班模块功放由ES8311内部控制
}

// ==================== 音频输入源切换 ====================

esp_err_t gpio_driver_set_audio_input(bool use_microphone)
{
    ESP_LOGW(TAG, "音频输入源切换被忽略 (路小班模块无SD_MODE引脚)");
    return ESP_OK;  // 路小班模块由ES8311自动处理麦克风/耳机
}
