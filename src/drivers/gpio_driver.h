/**
 * @file gpio_driver.h
 * @brief GPIO控制驱动头文件
 */

#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPIO控制引脚定义
 */
typedef enum {
    GPIO_CTRL_SD_MODE = 0,    // 麦克风/耳机切换
    GPIO_CTRL_PA_EN,          // 功放使能
    GPIO_CTRL_LED_STATUS,     // 状态LED
} gpio_ctrl_pin_t;

// ==================== 函数声明 ====================

/**
 * @brief 初始化GPIO驱动
 * @return ESP_OK成功
 */
esp_err_t gpio_driver_init(void);

/**
 * @brief 设置GPIO电平
 * @param pin GPIO引脚
 * @param level 电平(0/1)
 * @return ESP_OK成功
 */
esp_err_t gpio_driver_set_level(gpio_ctrl_pin_t pin, uint32_t level);

/**
 * @brief 获取GPIO电平
 * @param pin GPIO引脚
 * @param level 电平指针
 * @return ESP_OK成功
 */
esp_err_t gpio_driver_get_level(gpio_ctrl_pin_t pin, uint32_t *level);

/**
 * @brief 使能功放
 * @param enable true使能，false关闭
 * @return ESP_OK成功
 */
esp_err_t gpio_driver_enable_pa(bool enable);

/**
 * @brief 设置音频输入源
 * @param use_microphone true使用麦克风，false使用耳机输入
 * @return ESP_OK成功
 */
esp_err_t gpio_driver_set_audio_input(bool use_microphone);

#ifdef __cplusplus
}
#endif

#endif // GPIO_DRIVER_H
