#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AUTH_OK,
    AUTH_FAIL_INVALID_PIN,
    AUTH_FAIL_RATE_LIMITED,
    AUTH_FAIL_LOCKED_OUT,
} auth_result_t;

esp_err_t auth_init(void);
auth_result_t auth_verify_pin(const char *pin);
auth_result_t auth_set_pin(const char *old_pin, const char *new_pin);
bool auth_validate_pin_format(const char *pin);
bool auth_is_locked_out(void);
uint32_t auth_get_retry_delay_ms(void);
void auth_reset_failures(void);
