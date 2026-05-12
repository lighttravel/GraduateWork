#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2S_MCLK_PIN                  BOARD_I2S_MCLK_PIN
#define I2S_BCLK_PIN                  BOARD_I2S_BCLK_PIN
#define I2S_LRCK_PIN                  BOARD_I2S_LRCK_PIN
#define I2S_DIN_PIN                   BOARD_I2S_CODEC_DIN_PIN
#define I2S_DOUT_PIN                  BOARD_I2S_CODEC_DOUT_PIN

#define I2C_NUM                       BOARD_I2C_NUM
#define I2C_SCL_PIN                   BOARD_I2C_SCL_PIN
#define I2C_SDA_PIN                   BOARD_I2C_SDA_PIN
#define I2C_FREQ_HZ                   BOARD_I2C_FREQ_HZ
#define ES8311_I2C_ADDR               BOARD_ES8311_I2C_ADDR

#define AUDIO_PA_ENABLE_PIN           BOARD_AUDIO_PA_ENABLE_PIN
#define I2S_NUM                       I2S_NUM_0
#define I2S_SAMPLE_RATE               16000
#define I2S_SAMPLE_RATE_TTS           16000
#define I2S_BITS_PER_SAMPLE           I2S_BITS_PER_SAMPLE_16BIT
#define I2S_CHANNEL_NUM               1
#define I2S_DMA_BUF_COUNT             8
#define I2S_DMA_BUF_LEN               512

#define AUDIO_VOLUME                  BOARD_AUDIO_VOLUME
#define AUDIO_INPUT_GAIN_DB           BOARD_AUDIO_INPUT_GAIN_DB
#define VAD_THRESHOLD                 100
#define MAX_AUDIO_BUF_SIZE            (16 * 1024)

#define AUDIO_TTS_STREAM_BUFFER_BYTES (48 * 1024)
#define AUDIO_TTS_PREROLL_BYTES       8192
#define AUDIO_TTS_WRITE_CHUNK_BYTES   1024
#define AUDIO_TTS_CHUNK_WAIT_MS       80
#define AUDIO_TTS_DRAIN_TIMEOUT_MS    3000
#define AUDIO_TTS_ENQUEUE_TIMEOUT_MS  500

#define SYSTEM_TASK_STACK_SIZE        (8 * 1024)
#define SYSTEM_TASK_PRIORITY          5
#define AUDIO_TASK_STACK_SIZE         (12 * 1024)
#define AUDIO_TASK_PRIORITY           10

#ifdef __cplusplus
}
#endif

#endif
