#ifndef KEY_SERVICE_H
#define KEY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEY_ID_1 = 0,
    KEY_ID_2,
} key_id_t;

typedef struct {
    bool key1_pressed;
    bool key2_pressed;
} key_state_t;

esp_err_t key_service_init(void);
key_state_t key_service_read(void);
bool key_service_is_pressed(key_id_t key_id);

#ifdef __cplusplus
}
#endif

#endif
