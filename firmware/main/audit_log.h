#pragma once

#include "esp_err.h"
#include <stddef.h>

typedef enum {
    AUDIT_BOOT,
    AUDIT_AUTH_ATTEMPT,
    AUDIT_AUTH_LOCKOUT,
    AUDIT_PIN_CHANGE,
    AUDIT_FACTORY_RESET,
    AUDIT_FULL_RESET,
    AUDIT_BLE_CONNECT,
    AUDIT_BLE_DISCONNECT,
    AUDIT_OTA_START,
    AUDIT_OTA_SUCCESS,
    AUDIT_OTA_FAIL,
    AUDIT_SYSRQ,
} audit_event_t;

esp_err_t audit_log_init(void);
void audit_log_event(audit_event_t event, const char *details);
size_t audit_log_get_entries(char *buf, size_t buf_size);
void audit_log_clear(void);
esp_err_t audit_log_persist(void);
esp_err_t audit_log_load(void);
