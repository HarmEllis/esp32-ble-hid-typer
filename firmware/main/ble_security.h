#pragma once

#include "esp_err.h"
#include "host/ble_gap.h"
#include <stdint.h>

esp_err_t ble_security_init(void);
void ble_security_set_passkey(uint32_t passkey);
int ble_security_gap_event(struct ble_gap_event *event, void *arg);
