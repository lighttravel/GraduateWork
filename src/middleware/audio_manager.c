/**
 * @file audio_manager.c
 * @brief 音频管理器实现
 *
 * 使用新版统一音频驱动 (audio_driver)
 * 基于新版 ESP-IDF I2S/I2C API
 */

#include "audio_manager.h"
#include "audio_driver.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "AUDIO_MGR";

// ==================== 音频管理器上下文 ====================

/**
 * @brief 音频管理器上下文结构体
 */
typedef struct {
    audio_config_t config;          // 配置
    audio_state_t state;            // 状态
    uint8_t volume;                 // 当前音量
    bool vad_enabled;               // VAD启用状态
    uint16_t vad_threshold;         // VAD阈值

    // 新版统一音频驱动
    audio_driver_handle_t driver;   // 音频驱动句柄

    // 任务句柄
    TaskHandle_t record_task;       // 录音任务
    TaskHandle_t play_task;         // 播放任务

    // 播放缓冲区
    uint8_t *play_buffer;           // 播放缓冲区指针
    size_t play_buffer_size;        // 播放缓冲区大小
    size_t play_buffer_offset;      // 播放偏移量

    // 互斥锁
    SemaphoreHandle_t mutex;        // 互斥锁
} audio_manager_t;

static audio_manager_t *g_audio_mgr = NULL;

// ==================== 辅助函数 ====================

/**
 * @brief 计算音频能量(RMS)
 * @param data 音频数据
 * @param samples 采样点数
 * @return 能量值
 */
static uint32_t calculate_audio_energy(const int16_t *data, size_t samples)
{
    int64_t sum = 0;
    for (size_t i = 0; i < samples; i++) {
        sum += (int64_t)data[i] * data[i];
    }
    return (uint32_t)(sum / samples);
}

/**
 * @brief VAD检测 - 检测是否有人声
 * @param data 音频数据
 * @param samples 采样点数
 * @return true检测到人声
 */
static bool detect_voice_activity(const int16_t *data, size_t samples)
{
    if (!g_audio_mgr->vad_enabled) {
        return true;  // VAD未启用，始终返回有声音
    }

    uint32_t energy = calculate_audio_energy(data, samples);
    return (energy > g_audio_mgr->vad_threshold);
}

// ==================== 录音任务 ====================

/**
 * @brief 录音任务
 * @param pvParameters 参数(未使用)
 */
static void record_task(void *pvParameters)
{
    ESP_LOGI(TAG, "录音任务启动");

    // 录音缓冲区 (1024 个采样点 = 2048 字节)
    int16_t buffer[1024];
    const int samples = sizeof(buffer) / sizeof(int16_t);

    while (g_audio_mgr->state == AUDIO_STATE_RECORDING) {
        // 使用新驱动读取音频数据
        int samples_read = audio_driver_read(g_audio_mgr->driver, buffer, samples);

        if (samples_read > 0) {
            // VAD检测
            bool has_voice = detect_voice_activity(buffer, samples_read);

            if (has_voice && g_audio_mgr->config.data_cb) {
                // 回调音频数据
                g_audio_mgr->config.data_cb((uint8_t*)buffer, samples_read * sizeof(int16_t),
                                            g_audio_mgr->config.user_data);
            }
        } else if (samples_read < 0) {
            ESP_LOGE(TAG, "音频读取失败");
            break;
        }
    }

    ESP_LOGI(TAG, "录音任务结束");
    g_audio_mgr->state = AUDIO_STATE_IDLE;
    vTaskDelete(NULL);
}

// ==================== 播放任务 ====================

/**
 * @brief 播放任务
 * @param pvParameters 参数(未使用)
 */
static void play_task(void *pvParameters)
{
    ESP_LOGI(TAG, "播放任务启动");

    while (g_audio_mgr->state == AUDIO_STATE_PLAYING) {
        if (g_audio_mgr->play_buffer_offset >= g_audio_mgr->play_buffer_size) {
            // 播放完成
            ESP_LOGI(TAG, "播放完成");
            break;
        }

        // 计算本次要播放的数据量
        size_t remaining = g_audio_mgr->play_buffer_size - g_audio_mgr->play_buffer_offset;
        int samples_to_write = (remaining < 1024 * sizeof(int16_t)) ? (remaining / sizeof(int16_t)) : 1024;

        // 使用新驱动写入音频数据
        int samples_written = audio_driver_write(
            g_audio_mgr->driver,
            (const int16_t*)(g_audio_mgr->play_buffer + g_audio_mgr->play_buffer_offset),
            samples_to_write
        );

        if (samples_written > 0) {
            g_audio_mgr->play_buffer_offset += samples_written * sizeof(int16_t);
        } else {
            ESP_LOGE(TAG, "音频写入失败");
            break;
        }
    }

    // 停止播放后的清理工作
    if (g_audio_mgr->play_buffer) {
        free(g_audio_mgr->play_buffer);
        g_audio_mgr->play_buffer = NULL;
    }
    g_audio_mgr->play_buffer_size = 0;
    g_audio_mgr->play_buffer_offset = 0;

    // 停止播放模式
    audio_driver_stop_playback(g_audio_mgr->driver);

    ESP_LOGI(TAG, "播放任务结束");
    g_audio_mgr->state = AUDIO_STATE_IDLE;

    // 触发播放停止事件
    if (g_audio_mgr->config.event_cb) {
        g_audio_mgr->config.event_cb(AUDIO_EVENT_PLAY_STOP, g_audio_mgr->config.user_data);
    }

    vTaskDelete(NULL);
}

// ==================== 初始化和配置 ====================

esp_err_t audio_manager_init(const audio_config_t *config)
{
    if (g_audio_mgr != NULL) {
        ESP_LOGW(TAG, "音频管理器已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "初始化音频管理器 (新版驱动)");
    ESP_LOGI(TAG, "========================================");

    // 分配内存
    g_audio_mgr = (audio_manager_t *)calloc(1, sizeof(audio_manager_t));
    if (g_audio_mgr == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    // 保存配置
    if (config) {
        memcpy(&g_audio_mgr->config, config, sizeof(audio_config_t));
    }

    // 设置默认值
    g_audio_mgr->state = AUDIO_STATE_IDLE;
    g_audio_mgr->volume = config ? config->volume : AUDIO_VOLUME;
    g_audio_mgr->vad_enabled = config ? config->vad_enabled : false;
    g_audio_mgr->vad_threshold = config ? config->vad_threshold : VAD_THRESHOLD;

    // 创建互斥锁
    g_audio_mgr->mutex = xSemaphoreCreateMutex();
    if (g_audio_mgr->mutex == NULL) {
        ESP_LOGE(TAG, "互斥锁创建失败");
        free(g_audio_mgr);
        g_audio_mgr = NULL;
        return ESP_ERR_NO_MEM;
    }

    // 初始化统一音频驱动
    audio_driver_config_t driver_config = {
        // I2C 配置
        .i2c_sda_pin = I2C_SDA_PIN,
        .i2c_scl_pin = I2C_SCL_PIN,
        .i2c_freq_hz = I2C_FREQ_HZ,

        // I2S 配置
        .i2s_mclk_pin = I2S_MCLK_PIN,
        .i2s_bclk_pin = I2S_BCLK_PIN,
        .i2s_ws_pin = I2S_LRCK_PIN,
        .i2s_dout_pin = I2S_DIN_PIN,    // ESP32 TX -> ES8311 DIN (播放, GPIO11)
        .i2s_din_pin = I2S_DOUT_PIN,    // ESP32 RX <- ES8311 DOUT (录音, GPIO13)

        // 功放配置
        .pa_pin = GPIO_NUM_NC,  // 路小班模块无PA_EN引脚
        .pa_inverted = false,   // PA 高电平有效

        // 音频配置
        .input_sample_rate = I2S_SAMPLE_RATE,
        .output_sample_rate = I2S_SAMPLE_RATE_TTS,
        .default_volume = g_audio_mgr->volume,
        .input_gain = 30.0f,  // 30dB 输入增益

        // 功能配置
        .use_mclk = true,   // 使用外部MCLK (GPIO6), 与当前已验证硬件一致
        .use_filter = false, // 暂时禁用软件滤波器进行测试
        .es8311_addr = ES8311_I2C_ADDR,
    };

    esp_err_t ret = audio_driver_create(&driver_config, &g_audio_mgr->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "音频驱动初始化失败: %s", esp_err_to_name(ret));
        vSemaphoreDelete(g_audio_mgr->mutex);
        free(g_audio_mgr);
        g_audio_mgr = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "音频管理器初始化完成");
    ESP_LOGI(TAG, "========================================");

    return ESP_OK;
}

esp_err_t audio_manager_deinit(void)
{
    if (g_audio_mgr == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "反初始化音频管理器...");

    // 停止录音和播放
    audio_manager_stop_record();
    audio_manager_stop_play();

    // 销毁音频驱动
    if (g_audio_mgr->driver) {
        audio_driver_destroy(g_audio_mgr->driver);
        g_audio_mgr->driver = NULL;
    }

    // 删除互斥锁
    if (g_audio_mgr->mutex) {
        vSemaphoreDelete(g_audio_mgr->mutex);
    }

    // 释放内存
    free(g_audio_mgr);
    g_audio_mgr = NULL;

    ESP_LOGI(TAG, "音频管理器反初始化完成");
    return ESP_OK;
}

// ==================== 录音控制 ====================

esp_err_t audio_manager_start_record(void)
{
    if (g_audio_mgr == NULL) {
        ESP_LOGE(TAG, "音频管理器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (g_audio_mgr->state == AUDIO_STATE_RECORDING) {
        ESP_LOGW(TAG, "已在录音中");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "开始录音...");

    xSemaphoreTake(g_audio_mgr->mutex, portMAX_DELAY);

    // 启动录音模式
    esp_err_t ret = audio_driver_start_recording(g_audio_mgr->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动录音失败: %s", esp_err_to_name(ret));
        xSemaphoreGive(g_audio_mgr->mutex);
        return ret;
    }

    g_audio_mgr->state = AUDIO_STATE_RECORDING;

    // 创建录音任务
    BaseType_t task_ret = xTaskCreate(
        record_task,
        "record",
        AUDIO_TASK_STACK_SIZE,
        NULL,
        AUDIO_TASK_PRIORITY,
        &g_audio_mgr->record_task
    );

    xSemaphoreGive(g_audio_mgr->mutex);

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "录音任务创建失败");
        audio_driver_stop_recording(g_audio_mgr->driver);
        g_audio_mgr->state = AUDIO_STATE_IDLE;
        return ESP_FAIL;
    }

    // 触发录音开始事件
    if (g_audio_mgr->config.event_cb) {
        g_audio_mgr->config.event_cb(AUDIO_EVENT_RECORD_START, g_audio_mgr->config.user_data);
    }

    return ESP_OK;
}

esp_err_t audio_manager_stop_record(void)
{
    if (g_audio_mgr == NULL || g_audio_mgr->state != AUDIO_STATE_RECORDING) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止录音");

    xSemaphoreTake(g_audio_mgr->mutex, portMAX_DELAY);
    g_audio_mgr->state = AUDIO_STATE_IDLE;
    xSemaphoreGive(g_audio_mgr->mutex);

    // 停止录音模式
    audio_driver_stop_recording(g_audio_mgr->driver);

    // 等待录音任务结束
    if (g_audio_mgr->record_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        g_audio_mgr->record_task = NULL;
    }

    // 触发录音停止事件
    if (g_audio_mgr->config.event_cb) {
        g_audio_mgr->config.event_cb(AUDIO_EVENT_RECORD_STOP, g_audio_mgr->config.user_data);
    }

    return ESP_OK;
}

esp_err_t audio_manager_pause_record(void)
{
    ESP_LOGW(TAG, "暂停录音功能暂未实现");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_manager_resume_record(void)
{
    ESP_LOGW(TAG, "恢复录音功能暂未实现");
    return ESP_ERR_NOT_SUPPORTED;
}

// ==================== 播放控制 ====================

esp_err_t audio_manager_start_play(const uint8_t *data, size_t len)
{
    if (g_audio_mgr == NULL) {
        ESP_LOGE(TAG, "音频管理器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (g_audio_mgr->state == AUDIO_STATE_PLAYING) {
        ESP_LOGW(TAG, "已在播放中");
        return ESP_OK;
    }

    if (data == NULL || len == 0) {
        ESP_LOGE(TAG, "无效的播放数据");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "开始播放，数据长度: %d", len);

    xSemaphoreTake(g_audio_mgr->mutex, portMAX_DELAY);

    // 分配播放缓冲区
    g_audio_mgr->play_buffer = (uint8_t *)malloc(len);
    if (g_audio_mgr->play_buffer == NULL) {
        ESP_LOGE(TAG, "播放缓冲区分配失败");
        xSemaphoreGive(g_audio_mgr->mutex);
        return ESP_ERR_NO_MEM;
    }

    memcpy(g_audio_mgr->play_buffer, data, len);
    g_audio_mgr->play_buffer_size = len;
    g_audio_mgr->play_buffer_offset = 0;

    // 启动播放模式
    esp_err_t ret = audio_driver_start_playback(g_audio_mgr->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动播放失败: %s", esp_err_to_name(ret));
        free(g_audio_mgr->play_buffer);
        g_audio_mgr->play_buffer = NULL;
        xSemaphoreGive(g_audio_mgr->mutex);
        return ret;
    }

    g_audio_mgr->state = AUDIO_STATE_PLAYING;

    // 创建播放任务
    BaseType_t task_ret = xTaskCreate(
        play_task,
        "play",
        AUDIO_TASK_STACK_SIZE,
        NULL,
        AUDIO_TASK_PRIORITY,
        &g_audio_mgr->play_task
    );

    xSemaphoreGive(g_audio_mgr->mutex);

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "播放任务创建失败");
        free(g_audio_mgr->play_buffer);
        g_audio_mgr->play_buffer = NULL;
        audio_driver_stop_playback(g_audio_mgr->driver);
        g_audio_mgr->state = AUDIO_STATE_IDLE;
        return ESP_FAIL;
    }

    // 触发播放开始事件
    if (g_audio_mgr->config.event_cb) {
        g_audio_mgr->config.event_cb(AUDIO_EVENT_PLAY_START, g_audio_mgr->config.user_data);
    }

    return ESP_OK;
}

esp_err_t audio_manager_stop_play(void)
{
    if (g_audio_mgr == NULL || g_audio_mgr->state != AUDIO_STATE_PLAYING) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止播放");

    xSemaphoreTake(g_audio_mgr->mutex, portMAX_DELAY);
    g_audio_mgr->state = AUDIO_STATE_IDLE;
    xSemaphoreGive(g_audio_mgr->mutex);

    // 停止播放模式
    audio_driver_stop_playback(g_audio_mgr->driver);

    // 等待播放任务结束
    if (g_audio_mgr->play_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        g_audio_mgr->play_task = NULL;
    }

    return ESP_OK;
}

esp_err_t audio_manager_pause_play(void)
{
    ESP_LOGW(TAG, "暂停播放功能暂未实现");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_manager_resume_play(void)
{
    ESP_LOGW(TAG, "恢复播放功能暂未实现");
    return ESP_ERR_NOT_SUPPORTED;
}

// ==================== 音量控制 ====================

esp_err_t audio_manager_set_volume(uint8_t volume)
{
    if (volume > 100) {
        volume = 100;
    }

    if (g_audio_mgr) {
        g_audio_mgr->volume = volume;
        audio_driver_set_volume(g_audio_mgr->driver, volume);

        ESP_LOGI(TAG, "音量设置为: %d", volume);

        // 触发音量改变事件
        if (g_audio_mgr->config.event_cb) {
            g_audio_mgr->config.event_cb(AUDIO_EVENT_VOLUME_CHANGED, g_audio_mgr->config.user_data);
        }

        return ESP_OK;
    }

    return ESP_ERR_INVALID_STATE;
}

uint8_t audio_manager_get_volume(void)
{
    return g_audio_mgr ? g_audio_mgr->volume : 0;
}

// ==================== TTS音频播放 ====================

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/**
 * @brief 播放TTS音频数据（流式播放）
 *
 * 注意：I2S 配置为 STEREO 模式，audio_driver_write 内部会将单声道数据转换为立体声
 */
esp_err_t audio_manager_play_tts_audio(const uint8_t *data, size_t len)
{
    if (g_audio_mgr == NULL) {
        ESP_LOGE(TAG, "音频管理器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 检查驱动是否可用
    if (g_audio_mgr->driver == NULL) {
        ESP_LOGE(TAG, "音频驱动未初始化!");
        return ESP_ERR_INVALID_STATE;
    }

    // 确保处于播放模式
    if (g_audio_mgr->state != AUDIO_STATE_PLAYING) {
        ESP_LOGI(TAG, "自动切换到TTS播放模式");
        // 自动切换到播放模式
        esp_err_t ret = audio_driver_start_playback(g_audio_mgr->driver);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "启动播放失败: %s", esp_err_to_name(ret));
            return ret;
        }
        g_audio_mgr->state = AUDIO_STATE_PLAYING;
    }

    // 发送单声道数据到驱动，驱动内部会转换为立体声格式
    int samples = len / sizeof(int16_t);
    const int16_t *mono_data = (const int16_t *)data;

    // ===== 调试: 每次都打印 =====
    static int debug_counter = 0;
    debug_counter++;
    ESP_LOGI(TAG, "TTS音频 #%d: %d 字节, %d 采样点", debug_counter, (int)len, samples);

    // 直接写入单声道数据到 I2S
    ESP_LOGI(TAG, "调用 audio_driver_write...");
    int written = audio_driver_write(g_audio_mgr->driver, mono_data, samples);
    ESP_LOGI(TAG, "audio_driver_write 返回: %d", written);
    if (written < 0) {
        ESP_LOGE(TAG, "TTS音频写入失败");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief 开始TTS播放模式
 */
esp_err_t audio_manager_start_tts_playback(void)
{
    if (g_audio_mgr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "开始TTS播放模式");

    // 启动播放模式
    esp_err_t ret = audio_driver_start_playback(g_audio_mgr->driver);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动TTS播放失败: %s", esp_err_to_name(ret));
        return ret;
    }

    g_audio_mgr->state = AUDIO_STATE_PLAYING;

    return ESP_OK;
}

/**
 * @brief 停止TTS播放
 */
esp_err_t audio_manager_stop_tts_playback(void)
{
    if (g_audio_mgr == NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止TTS播放");

    // 停止播放模式
    audio_driver_stop_playback(g_audio_mgr->driver);

    g_audio_mgr->state = AUDIO_STATE_IDLE;

    return ESP_OK;
}

// ==================== TTS辅助函数 ====================

/**
 * @brief 启动播放（不带参数版本，用于TTS模块）
 */
esp_err_t audio_manager_start_playback(void)
{
    return audio_manager_start_tts_playback();
}

/**
 * @brief 停止播放（不带参数版本，用于TTS模块）
 */
esp_err_t audio_manager_stop_playback(void)
{
    return audio_manager_stop_tts_playback();
}

// ==================== 状态查询 ====================

audio_state_t audio_manager_get_state(void)
{
    return g_audio_mgr ? g_audio_mgr->state : AUDIO_STATE_IDLE;
}

bool audio_manager_is_recording(void)
{
    return g_audio_mgr ? g_audio_mgr->state == AUDIO_STATE_RECORDING : false;
}

bool audio_manager_is_playing(void)
{
    return g_audio_mgr ? g_audio_mgr->state == AUDIO_STATE_PLAYING : false;
}

// ==================== VAD控制 ====================

esp_err_t audio_manager_set_vad(bool enabled)
{
    if (g_audio_mgr) {
        g_audio_mgr->vad_enabled = enabled;
        ESP_LOGI(TAG, "VAD %s", enabled ? "启用" : "禁用");
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

esp_err_t audio_manager_set_vad_threshold(uint16_t threshold)
{
    if (g_audio_mgr) {
        g_audio_mgr->vad_threshold = threshold;
        ESP_LOGI(TAG, "VAD阈值设置为: %d", threshold);
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

// ==================== 诊断功能 ====================

void audio_manager_dump_codec_registers(void)
{
    if (g_audio_mgr == NULL || g_audio_mgr->driver == NULL) {
        ESP_LOGE(TAG, "无法转储寄存器: 音频管理器未初始化");
        return;
    }

    audio_driver_dump_codec_registers(g_audio_mgr->driver);
}
