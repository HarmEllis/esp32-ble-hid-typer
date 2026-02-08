#include "button_reset.h"
#include "neopixel.h"
#include "nvs_storage.h"
#include "audit_log.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

static const char *TAG = "button_reset";

#define BOOT_BUTTON_GPIO    GPIO_NUM_0
#define POLL_INTERVAL_MS    100
#define WARNING_START_MS    2000
#define RESET_TRIGGER_MS    10000

static void button_reset_task(void *pvParameters)
{
    TickType_t press_start = 0;
    bool was_pressed = false;
    bool reset_triggered = false;
    led_state_t saved_state = LED_STATE_OFF;

    while (1) {
        bool is_pressed = (gpio_get_level(BOOT_BUTTON_GPIO) == 0);

        if (is_pressed && !was_pressed) {
            press_start = xTaskGetTickCount();
            saved_state = neopixel_get_state();
            reset_triggered = false;
            ESP_LOGI(TAG, "BOOT button pressed - hold 10s to factory reset");
        }

        if (is_pressed && !reset_triggered) {
            uint32_t duration_ms = (xTaskGetTickCount() - press_start) * portTICK_PERIOD_MS;

            if (duration_ms >= WARNING_START_MS && duration_ms < RESET_TRIGGER_MS) {
                neopixel_set_state(LED_STATE_RESET_WARNING);
            }

            if (duration_ms >= RESET_TRIGGER_MS) {
                reset_triggered = true;
                ESP_LOGW(TAG, "Factory reset triggered via BOOT button");
                neopixel_set_state(LED_STATE_RESET_CONFIRMED);
                audit_log_event(AUDIT_FACTORY_RESET, "trigger=button");
                audit_log_persist();
                vTaskDelay(pdMS_TO_TICKS(1000));
                nvs_storage_factory_reset();
                esp_restart();
            }
        }

        if (!is_pressed && was_pressed && !reset_triggered) {
            ESP_LOGI(TAG, "BOOT button released - reset cancelled");
            neopixel_set_state(saved_state);
        }

        was_pressed = is_pressed;
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}

esp_err_t button_reset_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        return err;
    }

    BaseType_t ret = xTaskCreate(button_reset_task, "btn_reset", 2048, NULL, 3, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button reset task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Button reset monitor started (GPIO%d)", BOOT_BUTTON_GPIO);
    return ESP_OK;
}
