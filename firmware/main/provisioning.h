#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t provisioning_start(void);
bool provisioning_is_active(void);
