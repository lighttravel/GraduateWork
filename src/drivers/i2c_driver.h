/**
 * @file i2c_driver.h
 * @brief I2C驱动头文件(用于配置ES8311)
 */

#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化I2C主机
 * @return ESP_OK成功
 */
esp_err_t i2c_driver_init(void);

/**
 * @brief 反初始化I2C主机
 * @return ESP_OK成功
 */
esp_err_t i2c_driver_deinit(void);

/**
 * @brief 向I2C设备写入数据
 * @param device_addr I2C设备地址
 * @param reg_addr 寄存器地址
 * @param data 数据指针
 * @param len 数据长度
 * @return ESP_OK成功
 */
esp_err_t i2c_driver_write_bytes(uint8_t device_addr, uint8_t reg_addr,
                                  const uint8_t *data, size_t len);

/**
 * @brief 从I2C设备读取数据
 * @param device_addr I2C设备地址
 * @param reg_addr 寄存器地址
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return ESP_OK成功
 */
esp_err_t i2c_driver_read_bytes(uint8_t device_addr, uint8_t reg_addr,
                                 uint8_t *data, size_t len);

/**
 * @brief 向I2C设备写入单字节数据
 * @param device_addr I2C设备地址
 * @param reg_addr 寄存器地址
 * @param data 数据
 * @return ESP_OK成功
 */
esp_err_t i2c_driver_write_byte(uint8_t device_addr, uint8_t reg_addr, uint8_t data);

/**
 * @brief 从I2C设备读取单字节数据
 * @param device_addr I2C设备地址
 * @param reg_addr 寄存器地址
 * @param data 数据指针
 * @return ESP_OK成功
 */
esp_err_t i2c_driver_read_byte(uint8_t device_addr, uint8_t reg_addr, uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif // I2C_DRIVER_H
