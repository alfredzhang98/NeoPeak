#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t display_service_start(void);

// Force the panel and backlight off immediately. Used by the shutdown path so
// the screen goes dark the moment the long-press threshold is reached, even
// if blocking shutdown audio is still playing.
void display_service_blank(void);

#ifdef __cplusplus
}
#endif
