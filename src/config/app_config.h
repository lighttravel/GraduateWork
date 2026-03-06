#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_types.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 全局配置入口。需要改 GPIO、音量、阈值、Wi-Fi、云端密钥时，优先改这里。 */

/* -------------------------------------------------------------------------- */
/* GPIO 与总线映射                                                             */
/* -------------------------------------------------------------------------- */

#define I2S_MCLK_PIN                  GPIO_NUM_6
#define I2S_BCLK_PIN                  GPIO_NUM_14
#define I2S_LRCK_PIN                  GPIO_NUM_12
#define I2S_DIN_PIN                   GPIO_NUM_11
#define I2S_DOUT_PIN                  GPIO_NUM_13

#define I2C_NUM                       I2C_NUM_0
#define I2C_SCL_PIN                   GPIO_NUM_4
#define I2C_SDA_PIN                   GPIO_NUM_5
#define I2C_FREQ_HZ                   100000
#define ES8311_I2C_ADDR               0x18

#define AUDIO_PA_ENABLE_PIN           GPIO_NUM_NC
#define LED_STATUS_PIN                GPIO_NUM_47

/* -------------------------------------------------------------------------- */
/* 音频基础参数                                                                */
/* -------------------------------------------------------------------------- */

#define I2S_NUM                       I2S_NUM_0
#define I2S_SAMPLE_RATE               16000
#define I2S_SAMPLE_RATE_TTS           16000
#define I2S_BITS_PER_SAMPLE           I2S_BITS_PER_SAMPLE_16BIT
#define I2S_CHANNEL_NUM               1
#define I2S_DMA_BUF_COUNT             8
#define I2S_DMA_BUF_LEN               512

/*
 * AUDIO_VOLUME:
 *   板端扬声器音量，范围 0-100。
 *
 * AUDIO_INPUT_GAIN_DB:
 *   麦克风增益，单位 dB。越大越灵敏，但底噪也会更重。
 *
 * VAD_THRESHOLD:
 *   本地能量门限，越大越不容易触发。
 */
#define AUDIO_VOLUME                  55
#define AUDIO_INPUT_GAIN_DB           30.0f
#define VAD_THRESHOLD                 100
#define MAX_AUDIO_BUF_SIZE            (16 * 1024)

/*
 * TTS 流式播放缓冲参数。
 *
 * AUDIO_TTS_STREAM_BUFFER_BYTES:
 *   整体缓存大小。越大越稳，但更占内存。
 *
 * AUDIO_TTS_PREROLL_BYTES:
 *   开始播报前需要预存的 PCM 字节数。越大越不容易断续，但首音更慢。
 *
 * AUDIO_TTS_WRITE_CHUNK_BYTES:
 *   每次送入 I2S 的块大小。
 */
#define AUDIO_TTS_STREAM_BUFFER_BYTES (48 * 1024)
#define AUDIO_TTS_PREROLL_BYTES       8192
#define AUDIO_TTS_WRITE_CHUNK_BYTES   1024
#define AUDIO_TTS_CHUNK_WAIT_MS       80
#define AUDIO_TTS_DRAIN_TIMEOUT_MS    3000
#define AUDIO_TTS_ENQUEUE_TIMEOUT_MS  500

/* -------------------------------------------------------------------------- */
/* 唤醒                                                                        */
/* -------------------------------------------------------------------------- */

#define APP_WAKE_WORD_MODEL           "nihaoxiaozhi"
#define APP_WAKE_WORD_NAME            "你好小智"
#define APP_WAKE_WORD_THRESHOLD       0.20f
#define APP_WAKE_WORD_VAD_THRESHOLD   0.20f

/* -------------------------------------------------------------------------- */
/* 主流程时序                                                                  */
/* -------------------------------------------------------------------------- */

#define APP_ASR_RECORD_SECONDS        6
#define APP_ASR_FINAL_TIMEOUT_MS      12000
#define APP_CHAT_TIMEOUT_MS           30000
#define APP_TTS_TIMEOUT_MS            90000
#define APP_REPLY_TEXT_MAX_LEN        1024
#define APP_TTS_REPLY_MAX_CHARS       120
#define APP_TTS_REPLY_MAX_SENTENCES   3
#define APP_CHAT_MAX_TOKENS           180
#define APP_CHAT_SYSTEM_PROMPT        "你是小智，一个中文语音助手。回答必须适合直接语音播报：只用纯文本，不要Markdown、标题、列表、编号、表格、链接或表情。先直接回答结论，再补充一到两句必要信息，总长度控制在120个汉字以内。"
#define APP_FALLBACK_REPLY            "我没听清，请再说一遍。"

/* -------------------------------------------------------------------------- */
/* Wi-Fi                                                                       */
/* -------------------------------------------------------------------------- */

#define DEFAULT_WIFI_SSID             "XUNTIAN_2.4G"
#define DEFAULT_WIFI_PASSWORD         "xuntian13937020766"
#define WIFI_AP_SSID                  "ESP32-Xiaozhi"
#define WIFI_AP_PASSWORD              ""
#define WIFI_AP_CHANNEL               1
#define WIFI_AP_MAX_CONN              4
#define WIFI_CONNECT_TIMEOUT_MS       30000
#define WIFI_RETRY_COUNT              5

/* -------------------------------------------------------------------------- */
/* 云端服务                                                                    */
/* -------------------------------------------------------------------------- */

#define IFLYTEK_APPID                 "9ed12221"
#define IFLYTEK_API_KEY               "b1ffeca6c122160445ebd4a4d69003b4"
#define IFLYTEK_API_SECRET            "NmYwODk4ODVlNGE2YWZhNGM2YjhmMjE4"
#define IFLYTEK_ASR_LANGUAGE          "zh_cn"
#define IFLYTEK_ASR_DOMAIN            "iat"

/* 讯飞 TTS 请求参数。这里控制的是云端合成参数，不是功放音量。 */
#define IFLYTEK_TTS_VOICE             "xiaoyan"
#define IFLYTEK_TTS_SPEED             50
#define IFLYTEK_TTS_REQUEST_VOLUME    50
#define IFLYTEK_TTS_PITCH             50

#define DEEPSEEK_API_URL              "https://api.deepseek.com/v1/chat/completions"
#define DEEPSEEK_API_HOST             "api.deepseek.com"
#define DEEPSEEK_API_PORT             443
#define DEEPSEEK_API_KEY              "sk-e858af6399024030a35798cac18a961a"
#define DEEPSEEK_MODEL                "deepseek-chat"

/* -------------------------------------------------------------------------- */
/* 通用长度与任务栈                                                             */
/* -------------------------------------------------------------------------- */

#define MAX_SSID_LEN                  32
#define MAX_PASSWORD_LEN              64
#define MAX_API_KEY_LEN               128
#define MAX_URL_LEN                   256
#define MAX_DEVICE_NAME_LEN           32

#define MAX_CHAT_HISTORY              6
#define MAX_MESSAGE_LEN               1024
#define MAX_RESPONSE_LEN              2048

#define SYSTEM_TASK_STACK_SIZE        (8 * 1024)
#define SYSTEM_TASK_PRIORITY          5
#define AUDIO_TASK_STACK_SIZE         (12 * 1024)
#define AUDIO_TASK_PRIORITY           10
#define HTTP_TASK_STACK_SIZE          (4 * 1024)
#define HTTP_TASK_PRIORITY            8

#define LOG_LOCAL_LEVEL               ESP_LOG_INFO

#ifdef __cplusplus
}
#endif

#endif
