#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t ble_server_init(void);
void ble_server_stop(void);
bool ble_server_is_connected(void);
void ble_server_notify_status(void);
void ble_server_notify_progress(uint32_t current, uint32_t total);
