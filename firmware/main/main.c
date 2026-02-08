#include <stdio.h>
#include "esp_log.h"
#include "nvs_storage.h"
#include "neopixel.h"
#include "usb_hid.h"
#include "typing_engine.h"
#include "auth.h"
#include "audit_log.h"
#include "provisioning.h"
#include "ble_server.h"
#include "button_reset.h"
#include "serial_cmd.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 BLE HID Typer starting...");

    /* Initialize encrypted NVS */
    ESP_ERROR_CHECK(nvs_storage_init());

    /* Initialize NeoPixel LED */
    ESP_ERROR_CHECK(neopixel_init());

    /* Initialize audit logging */
    ESP_ERROR_CHECK(audit_log_init());
    audit_log_load();
    audit_log_event(AUDIT_BOOT, NULL);

    /* Initialize BOOT button monitor (both modes) */
    ESP_ERROR_CHECK(button_reset_init());

    /* Initialize serial console commands (both modes) */
    ESP_ERROR_CHECK(serial_cmd_init());

    if (!nvs_storage_has_pin()) {
        ESP_LOGI(TAG, "No PIN found - entering provisioning mode");
        provisioning_start();
        return;
    }

    ESP_LOGI(TAG, "PIN found - entering normal mode");

    /* Initialize auth */
    ESP_ERROR_CHECK(auth_init());

    /* Initialize USB HID keyboard */
    ESP_ERROR_CHECK(usb_hid_init());

    /* Initialize typing engine */
    ESP_ERROR_CHECK(typing_engine_init());

    neopixel_set_state(LED_STATE_OFF);

    /* Initialize BLE server (normal mode) */
    ESP_ERROR_CHECK(ble_server_init());

    ESP_LOGI(TAG, "Normal mode initialized");
}
