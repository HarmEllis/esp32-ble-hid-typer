#include "serial_cmd.h"
#include "nvs_storage.h"
#include "audit_log.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "serial_cmd";

#define CMD_BUF_SIZE 128

static void cmd_status(void)
{
    printf("Status: running\n");
    printf("Free heap: %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    printf("Min free heap: %lu bytes\n", (unsigned long)esp_get_minimum_free_heap_size());
    printf("PIN set: %s\n", nvs_storage_has_pin() ? "yes" : "no");
}

static void cmd_heap(void)
{
    printf("Free heap: %lu bytes\n", (unsigned long)esp_get_free_heap_size());
    printf("Min free heap: %lu bytes\n", (unsigned long)esp_get_minimum_free_heap_size());
    printf("Largest free block: %lu bytes\n",
           (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

static void cmd_factory_reset(void)
{
    printf("Factory reset in progress...\n");
    audit_log_event(AUDIT_FACTORY_RESET, "trigger=serial");
    audit_log_persist();
    nvs_storage_factory_reset();
    esp_restart();
}

static void cmd_full_reset(void)
{
    printf("Full reset in progress...\n");
    audit_log_event(AUDIT_FULL_RESET, "trigger=serial");
    audit_log_persist();
    nvs_storage_full_reset();
    esp_restart();
}

static void cmd_reboot(void)
{
    printf("Rebooting...\n");
    audit_log_persist();
    esp_restart();
}

static void cmd_help(void)
{
    printf("Commands:\n");
    printf("  status           - Show device status\n");
    printf("  heap             - Show heap usage\n");
    printf("  factory_reset    - Wipe PIN/WiFi, reboot to provisioning\n");
    printf("  full_reset       - Wipe everything, reboot to provisioning\n");
    printf("  reboot           - Reboot device\n");
    printf("  help             - Show this help\n");
}

static void process_command(const char *cmd)
{
    /* Trim trailing whitespace */
    char buf[CMD_BUF_SIZE];
    strncpy(buf, cmd, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' || buf[len - 1] == ' ')) {
        buf[--len] = '\0';
    }
    if (len == 0) return;

    if (strcmp(buf, "status") == 0) {
        cmd_status();
    } else if (strcmp(buf, "heap") == 0) {
        cmd_heap();
    } else if (strcmp(buf, "factory_reset") == 0) {
        cmd_factory_reset();
    } else if (strcmp(buf, "full_reset") == 0) {
        cmd_full_reset();
    } else if (strcmp(buf, "reboot") == 0) {
        cmd_reboot();
    } else if (strcmp(buf, "help") == 0) {
        cmd_help();
    } else {
        printf("Unknown command: %s\nType 'help' for available commands.\n", buf);
    }
}

static void serial_cmd_task(void *pvParameters)
{
    char buf[CMD_BUF_SIZE];
    int pos = 0;

    while (1) {
        int c = getchar();
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (c == '\n' || c == '\r') {
            if (pos > 0) {
                buf[pos] = '\0';
                process_command(buf);
                pos = 0;
            }
        } else if (pos < (int)(sizeof(buf) - 1)) {
            buf[pos++] = (char)c;
        }
    }
}

esp_err_t serial_cmd_init(void)
{
    BaseType_t ret = xTaskCreate(serial_cmd_task, "serial_cmd", 3072, NULL, 2, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create serial command task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Serial command console started (type 'help')");
    return ESP_OK;
}
