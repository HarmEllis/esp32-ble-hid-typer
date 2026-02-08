#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

esp_err_t usb_hid_init(void);
bool usb_hid_ready(void);
esp_err_t usb_hid_send_key(uint8_t modifier, uint8_t keycode);
esp_err_t usb_hid_release_keys(void);
