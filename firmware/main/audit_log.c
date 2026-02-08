#include "audit_log.h"
#include "nvs_storage.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "audit_log";

#define AUDIT_BUF_SIZE  4096
#define NS_AUDIT        "audit"
#define MAX_ENTRY_LEN   256

static char s_buffer[AUDIT_BUF_SIZE];
static size_t s_write_pos;
static bool s_wrapped;

static const char *event_name(audit_event_t event)
{
    switch (event) {
    case AUDIT_BOOT:            return "boot";
    case AUDIT_AUTH_ATTEMPT:    return "auth_attempt";
    case AUDIT_AUTH_LOCKOUT:    return "auth_lockout";
    case AUDIT_PIN_CHANGE:      return "pin_change";
    case AUDIT_FACTORY_RESET:   return "factory_reset";
    case AUDIT_FULL_RESET:      return "full_reset";
    case AUDIT_BLE_CONNECT:     return "ble_connect";
    case AUDIT_BLE_DISCONNECT:  return "ble_disconnect";
    case AUDIT_OTA_START:       return "ota_start";
    case AUDIT_OTA_SUCCESS:     return "ota_success";
    case AUDIT_OTA_FAIL:        return "ota_fail";
    case AUDIT_SYSRQ:           return "sysrq";
    default:                    return "unknown";
    }
}

esp_err_t audit_log_init(void)
{
    memset(s_buffer, 0, sizeof(s_buffer));
    s_write_pos = 0;
    s_wrapped = false;

    /* Register shutdown handler to persist log */
    esp_register_shutdown_handler(audit_log_persist);

    ESP_LOGI(TAG, "Audit log initialized (%d bytes buffer)", AUDIT_BUF_SIZE);
    return ESP_OK;
}

void audit_log_event(audit_event_t event, const char *details)
{
    char entry[MAX_ENTRY_LEN];

    /* Uptime in seconds */
    int64_t uptime_us = esp_timer_get_time();
    uint32_t uptime_s = (uint32_t)(uptime_us / 1000000);
    uint32_t hours = uptime_s / 3600;
    uint32_t mins = (uptime_s % 3600) / 60;
    uint32_t secs = uptime_s % 60;

    /* Syslog RFC5424 format */
    int len;
    if (details && details[0]) {
        len = snprintf(entry, sizeof(entry),
                       "<134>1 %02lu:%02lu:%02lu esp32-hid - %s - - %s\n",
                       (unsigned long)hours, (unsigned long)mins, (unsigned long)secs,
                       event_name(event), details);
    } else {
        len = snprintf(entry, sizeof(entry),
                       "<134>1 %02lu:%02lu:%02lu esp32-hid - %s - -\n",
                       (unsigned long)hours, (unsigned long)mins, (unsigned long)secs,
                       event_name(event));
    }

    if (len <= 0 || len >= (int)sizeof(entry)) return;

    /* Write to ring buffer */
    for (int i = 0; i < len; i++) {
        s_buffer[s_write_pos] = entry[i];
        s_write_pos = (s_write_pos + 1) % AUDIT_BUF_SIZE;
        if (s_write_pos == 0) s_wrapped = true;
    }

    ESP_LOGD(TAG, "Logged: %s %s", event_name(event), details ? details : "");
}

size_t audit_log_get_entries(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) return 0;

    size_t data_size;
    size_t start;

    if (s_wrapped) {
        data_size = AUDIT_BUF_SIZE;
        start = s_write_pos;
    } else {
        data_size = s_write_pos;
        start = 0;
    }

    size_t copy_size = data_size < buf_size - 1 ? data_size : buf_size - 1;

    for (size_t i = 0; i < copy_size; i++) {
        buf[i] = s_buffer[(start + i) % AUDIT_BUF_SIZE];
    }
    buf[copy_size] = '\0';

    return copy_size;
}

void audit_log_clear(void)
{
    memset(s_buffer, 0, sizeof(s_buffer));
    s_write_pos = 0;
    s_wrapped = false;
    nvs_storage_erase_key(NS_AUDIT, "log_data");
    nvs_storage_erase_key(NS_AUDIT, "log_pos");
    nvs_storage_erase_key(NS_AUDIT, "log_wrap");
    ESP_LOGI(TAG, "Audit log cleared");
}

esp_err_t audit_log_persist(void)
{
    esp_err_t err;
    err = nvs_storage_set_blob(NS_AUDIT, "log_data", s_buffer, AUDIT_BUF_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist log data: %s", esp_err_to_name(err));
        return err;
    }

    uint16_t pos = (uint16_t)s_write_pos;
    nvs_storage_set_u16(NS_AUDIT, "log_pos", pos);
    nvs_storage_set_u8(NS_AUDIT, "log_wrap", s_wrapped ? 1 : 0);

    ESP_LOGI(TAG, "Audit log persisted (%u bytes, pos=%u, wrapped=%d)",
             AUDIT_BUF_SIZE, pos, s_wrapped);
    return ESP_OK;
}

esp_err_t audit_log_load(void)
{
    size_t len = AUDIT_BUF_SIZE;
    esp_err_t err = nvs_storage_get_blob(NS_AUDIT, "log_data", s_buffer, &len);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No persisted audit log found");
        return ESP_OK;  /* Not an error — first boot */
    }

    uint16_t pos = 0;
    nvs_storage_get_u16(NS_AUDIT, "log_pos", &pos);
    s_write_pos = pos;

    uint8_t wrap = 0;
    nvs_storage_get_u8(NS_AUDIT, "log_wrap", &wrap);
    s_wrapped = (wrap != 0);

    ESP_LOGI(TAG, "Audit log loaded (pos=%u, wrapped=%d)", pos, s_wrapped);
    return ESP_OK;
}
