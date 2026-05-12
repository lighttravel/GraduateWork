#ifndef ESP_LOG_H
#define ESP_LOG_H

#define ESP_LOGI(tag, fmt, ...)
#define ESP_LOGW(tag, fmt, ...)
#define ESP_LOGE(tag, fmt, ...)

static inline const char *esp_err_to_name(int err)
{
    (void)err;
    return "ERR";
}

#endif
