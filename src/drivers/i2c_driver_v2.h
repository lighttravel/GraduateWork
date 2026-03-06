/**
 * @file i2c_driver_v2.h
 * @brief I2C驱动实现 (新版 - 基于 i2c_master_bus)
 *
 * 使用新版 ESP-IDF I2C 驱动 API
 * 更稳定的总线管理和更好的错误处理
 */

#ifndef I2C_DRIVER_V2_H
#define I2C_DRIVER_V2_H

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I2C 总线配置结构体
 */
typedef struct {
    i2c_port_t port;          ///< I2C 端口号
    gpio_num_t sda_pin;       ///< SDA 引脚
    gpio_num_t scl_pin;       ///< SCL 引脚
    uint32_t clk_speed_hz;    ///< 时钟频率 (Hz)
    bool enable_pullup;       ///< 是否启用内部上拉
} i2c_driver_v2_config_t;

/**
 * @brief 初始化 I2C 总线
 *
 * @param config 配置参数
 * @param p_bus_handle 返回的总线句柄指针
 * @return esp_err_t
 */
esp_err_t i2c_driver_v2_init(const i2c_driver_v2_config_t *config,
                              i2c_master_bus_handle_t *p_bus_handle);

/**
 * @brief 反初始化 I2C 总线
 *
 * @param bus_handle 总线句柄
 * @return esp_err_t
 */
esp_err_t i2c_driver_v2_deinit(i2c_master_bus_handle_t bus_handle);

/**
 * @brief 扫描 I2C 总线上的设备
 *
 * @param bus_handle 总线句柄
 * @param found_addr 返回找到的设备地址数组
 * @param max_devices 数组最大长度
 * @param num_found 返回找到的设备数量
 * @return esp_err_t
 */
esp_err_t i2c_driver_v2_scan(i2c_master_bus_handle_t bus_handle,
                              uint8_t *found_addr, int max_devices, int *num_found);

/**
 * @brief 检测设备是否存在
 *
 * @param bus_handle 总线句柄
 * @param device_addr 设备地址
 * @return true 设备存在
 * @return false 设备不存在
 */
bool i2c_driver_v2_probe(i2c_master_bus_handle_t bus_handle, uint8_t device_addr);

/**
 * @brief 创建设备句柄
 *
 * @param bus_handle 总线句柄
 * @param device_addr 设备地址
 * @param p_dev_handle 返回的设备句柄
 * @return esp_err_t
 */
esp_err_t i2c_driver_v2_device_add(i2c_master_bus_handle_t bus_handle,
                                    uint8_t device_addr,
                                    i2c_master_dev_handle_t *p_dev_handle);

/**
 * @brief 删除设备句柄
 *
 * @param dev_handle 设备句柄
 * @return esp_err_t
 */
esp_err_t i2c_driver_v2_device_remove(i2c_master_dev_handle_t dev_handle);

/**
 * @brief 写入单字节到寄存器
 *
 * @param dev_handle 设备句柄
 * @param reg_addr 寄存器地址
 * @param data 数据
 * @return esp_err_t
 */
esp_err_t i2c_driver_v2_write_byte(i2c_master_dev_handle_t dev_handle,
                                    uint8_t reg_addr, uint8_t data);

/**
 * @brief 读取单字节寄存器
 *
 * @param dev_handle 设备句柄
 * @param reg_addr 寄存器地址
 * @param data 返回的数据
 * @return esp_err_t
 */
esp_err_t i2c_driver_v2_read_byte(i2c_master_dev_handle_t dev_handle,
                                   uint8_t reg_addr, uint8_t *data);

/**
 * @brief 写入多字节到寄存器
 *
 * @param dev_handle 设备句柄
 * @param reg_addr 起始寄存器地址
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return esp_err_t
 */
esp_err_t i2c_driver_v2_write_bytes(i2c_master_dev_handle_t dev_handle,
                                     uint8_t reg_addr, const uint8_t *data, size_t len);

/**
 * @brief 读取多字节寄存器
 *
 * @param dev_handle 设备句柄
 * @param reg_addr 起始寄存器地址
 * @param data 返回的数据缓冲区
 * @param len 读取长度
 * @return esp_err_t
 */
esp_err_t i2c_driver_v2_read_bytes(i2c_master_dev_handle_t dev_handle,
                                    uint8_t reg_addr, uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // I2C_DRIVER_V2_H
