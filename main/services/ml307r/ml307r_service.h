#ifndef ML307R_SERVICE_H
#define ML307R_SERVICE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ml307r_service_init(void);
esp_err_t ml307r_service_probe(void);
esp_err_t ml307r_service_check_network(void);

#ifdef __cplusplus
}
#endif

#endif
