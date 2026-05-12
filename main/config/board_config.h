#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_types.h"
#include "driver/uart.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_I2S_MCLK_PIN            GPIO_NUM_6
#define BOARD_I2S_BCLK_PIN            GPIO_NUM_14
#define BOARD_I2S_LRCK_PIN            GPIO_NUM_12
#define BOARD_I2S_CODEC_DIN_PIN       GPIO_NUM_11
#define BOARD_I2S_CODEC_DOUT_PIN      GPIO_NUM_13

#define BOARD_I2C_NUM                 I2C_NUM_0
#define BOARD_I2C_SCL_PIN             GPIO_NUM_4
#define BOARD_I2C_SDA_PIN             GPIO_NUM_5
#define BOARD_I2C_FREQ_HZ             100000
#define BOARD_ES8311_I2C_ADDR         0x18

#define BOARD_KEY1_PIN                GPIO_NUM_8
#define BOARD_KEY2_PIN                GPIO_NUM_3
#define BOARD_KEY_ACTIVE_LEVEL        0

#define BOARD_ML307R_UART_NUM         UART_NUM_1
#define BOARD_ML307R_RX_PIN           GPIO_NUM_9
#define BOARD_ML307R_TX_PIN           GPIO_NUM_10
#define BOARD_ML307R_BAUD_RATE        115200
#define BOARD_ML307R_RX_BUF_SIZE      2048
#define BOARD_ML307R_TX_BUF_SIZE      512
#define BOARD_ML307R_AT_TIMEOUT_MS    1000

#define BOARD_TFT_RST_PIN             GPIO_NUM_7
#define BOARD_TFT_SCLK_PIN            GPIO_NUM_15
#define BOARD_TFT_MOSI_PIN            GPIO_NUM_16
#define BOARD_TFT_DC_PIN              GPIO_NUM_17
#define BOARD_TFT_CS_PIN              GPIO_NUM_18
#define BOARD_TFT_BL_PIN              GPIO_NUM_47
#define BOARD_TFT_ENABLED             0

#define BOARD_AUDIO_PA_ENABLE_PIN     GPIO_NUM_NC
#define BOARD_AUDIO_VOLUME            55
#define BOARD_AUDIO_INPUT_GAIN_DB     30.0f

#ifdef __cplusplus
}
#endif

#endif
