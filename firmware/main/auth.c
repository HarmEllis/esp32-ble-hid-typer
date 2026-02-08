#include "auth.h"
#include "nvs_storage.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "auth";

#define NS_AUTH         "auth"
#define MAX_ATTEMPTS_PER_WINDOW  3
#define WINDOW_MS               60000   /* 60 seconds */
#define LOCKOUT_THRESHOLD       10
#define BASE_BACKOFF_MS         60000   /* 60 seconds */

static uint8_t s_fail_count;
static int64_t s_last_fail_time;  /* microseconds from esp_timer_get_time */
static bool s_locked_out;

esp_err_t auth_init(void)
{
    nvs_storage_get_u8(NS_AUTH, "fail_count", &s_fail_count);
    nvs_storage_get_i64(NS_AUTH, "fail_time", &s_last_fail_time);

    uint8_t lockout = 0;
    nvs_storage_get_u8(NS_AUTH, "lockout", &lockout);
    s_locked_out = (lockout != 0);

    if (s_locked_out) {
        ESP_LOGW(TAG, "Device is locked out after %d failed attempts", s_fail_count);
    }
    return ESP_OK;
}

bool auth_validate_pin_format(const char *pin)
{
    if (pin == NULL) return false;
    size_t len = strlen(pin);
    if (len != 6) return false;

    /* Must be all digits */
    for (int i = 0; i < 6; i++) {
        if (!isdigit((unsigned char)pin[i])) return false;
    }

    /* Must not be 000000 */
    if (strcmp(pin, "000000") == 0) return false;

    /* Must not be sequential */
    if (strcmp(pin, "123456") == 0 || strcmp(pin, "654321") == 0) return false;

    /* Must not be all same digit */
    bool all_same = true;
    for (int i = 1; i < 6; i++) {
        if (pin[i] != pin[0]) {
            all_same = false;
            break;
        }
    }
    if (all_same) return false;

    return true;
}

bool auth_is_locked_out(void)
{
    return s_locked_out;
}

uint32_t auth_get_retry_delay_ms(void)
{
    if (s_locked_out) return UINT32_MAX;
    if (s_fail_count < MAX_ATTEMPTS_PER_WINDOW) return 0;

    int64_t now = esp_timer_get_time();
    int64_t elapsed_ms = (now - s_last_fail_time) / 1000;

    /* Exponential backoff: 60s, 120s, 240s, ... */
    uint32_t backoff_ms = BASE_BACKOFF_MS;
    for (int i = MAX_ATTEMPTS_PER_WINDOW; i < s_fail_count && i < LOCKOUT_THRESHOLD; i++) {
        backoff_ms *= 2;
        if (backoff_ms > 3600000) {
            backoff_ms = 3600000;  /* Cap at 1 hour */
            break;
        }
    }

    if (elapsed_ms >= backoff_ms) return 0;
    return (uint32_t)(backoff_ms - elapsed_ms);
}

static void record_failure(void)
{
    s_fail_count++;
    s_last_fail_time = esp_timer_get_time();

    nvs_storage_set_u8(NS_AUTH, "fail_count", s_fail_count);
    nvs_storage_set_i64(NS_AUTH, "fail_time", s_last_fail_time);

    if (s_fail_count >= LOCKOUT_THRESHOLD) {
        s_locked_out = true;
        nvs_storage_set_u8(NS_AUTH, "lockout", 1);
        ESP_LOGE(TAG, "Device locked out after %d failures", s_fail_count);
    } else {
        ESP_LOGW(TAG, "PIN failure %d/%d", s_fail_count, LOCKOUT_THRESHOLD);
    }
}

static void record_success(void)
{
    s_fail_count = 0;
    s_last_fail_time = 0;
    nvs_storage_set_u8(NS_AUTH, "fail_count", 0);
    nvs_storage_set_i64(NS_AUTH, "fail_time", 0);
}

auth_result_t auth_verify_pin(const char *pin)
{
    if (s_locked_out) return AUTH_FAIL_LOCKED_OUT;
    if (auth_get_retry_delay_ms() > 0) return AUTH_FAIL_RATE_LIMITED;

    char stored_pin[7] = {0};
    size_t len = sizeof(stored_pin);
    esp_err_t err = nvs_storage_get_pin(stored_pin, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read stored PIN");
        return AUTH_FAIL_INVALID_PIN;
    }

    if (strcmp(pin, stored_pin) == 0) {
        record_success();
        return AUTH_OK;
    }

    record_failure();
    return AUTH_FAIL_INVALID_PIN;
}

auth_result_t auth_set_pin(const char *old_pin, const char *new_pin)
{
    if (s_locked_out) return AUTH_FAIL_LOCKED_OUT;

    /* Verify old PIN */
    auth_result_t result = auth_verify_pin(old_pin);
    if (result != AUTH_OK) return result;

    /* Validate new PIN format */
    if (!auth_validate_pin_format(new_pin)) return AUTH_FAIL_INVALID_PIN;

    /* Store new PIN */
    esp_err_t err = nvs_storage_set_pin(new_pin);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store new PIN");
        return AUTH_FAIL_INVALID_PIN;
    }

    ESP_LOGI(TAG, "PIN changed successfully");
    return AUTH_OK;
}

void auth_reset_failures(void)
{
    s_fail_count = 0;
    s_last_fail_time = 0;
    s_locked_out = false;
    nvs_storage_set_u8(NS_AUTH, "fail_count", 0);
    nvs_storage_set_i64(NS_AUTH, "fail_time", 0);
    nvs_storage_set_u8(NS_AUTH, "lockout", 0);
    ESP_LOGI(TAG, "Auth failures reset");
}
