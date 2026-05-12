#ifndef ESP_CHECK_H
#define ESP_CHECK_H

#define ESP_RETURN_ON_ERROR(expr, tag, msg) do { int _ret = (expr); if (_ret != ESP_OK) return _ret; } while (0)

#endif
