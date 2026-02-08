#include "neopixel.h"
#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "neopixel";

#define LED_STRIP_GPIO      48
#define LED_STRIP_RMT_RES   (10 * 1000 * 1000)  /* 10 MHz */
#define DEFAULT_BRIGHTNESS  5

static led_strip_handle_t s_strip;
static led_state_t s_state = LED_STATE_OFF;
static uint8_t s_brightness = DEFAULT_BRIGHTNESS;
static TaskHandle_t s_task_handle;

static void set_color(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t br = s_brightness;
    led_strip_set_pixel(s_strip, 0,
                        (r * br) / 100,
                        (g * br) / 100,
                        (b * br) / 100);
    led_strip_refresh(s_strip);
}

static void led_off(void)
{
    led_strip_clear(s_strip);
}

static void neopixel_task(void *arg)
{
    led_state_t prev = LED_STATE_OFF;
    bool on = false;

    while (1) {
        led_state_t cur = s_state;

        /* Reset blink phase on state change */
        if (cur != prev) {
            on = false;
            prev = cur;
        }

        switch (cur) {
        case LED_STATE_OFF:
            led_off();
            vTaskDelay(pdMS_TO_TICKS(200));
            break;

        case LED_STATE_PROVISIONING:
            /* Orange slow blink: 1s on, 1s off */
            if (on) {
                set_color(255, 165, 0);
            } else {
                led_off();
            }
            on = !on;
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;

        case LED_STATE_BLE_CONNECTED:
            set_color(0, 0, 255);
            vTaskDelay(pdMS_TO_TICKS(200));
            break;

        case LED_STATE_WIFI_CONNECTED:
            set_color(255, 255, 255);
            vTaskDelay(pdMS_TO_TICKS(200));
            break;

        case LED_STATE_WSS_CONNECTED:
            set_color(255, 255, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
            break;

        case LED_STATE_TYPING:
            /* Red flash: 500ms on/off */
            if (on) {
                set_color(255, 0, 0);
            } else {
                led_off();
            }
            on = !on;
            vTaskDelay(pdMS_TO_TICKS(500));
            break;

        case LED_STATE_RESET_WARNING:
            /* Yellow rapid flash: 100ms on/off */
            if (on) {
                set_color(255, 255, 0);
            } else {
                led_off();
            }
            on = !on;
            vTaskDelay(pdMS_TO_TICKS(100));
            break;

        case LED_STATE_RESET_CONFIRMED:
            set_color(255, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
            break;

        case LED_STATE_ERROR:
            /* Red rapid blink: 100ms on/off */
            if (on) {
                set_color(255, 0, 0);
            } else {
                led_off();
            }
            on = !on;
            vTaskDelay(pdMS_TO_TICKS(100));
            break;

        case LED_STATE_OTA:
            /* Purple pulsing — simple blink for now */
            if (on) {
                set_color(128, 0, 255);
            } else {
                led_off();
            }
            on = !on;
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        }
    }
}

esp_err_t neopixel_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = LED_STRIP_RMT_RES,
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init LED strip: %s", esp_err_to_name(err));
        return err;
    }

    led_strip_clear(s_strip);

    xTaskCreate(neopixel_task, "neopixel", 2048, NULL, 5, &s_task_handle);
    ESP_LOGI(TAG, "NeoPixel initialized on GPIO%d, brightness %d%%", LED_STRIP_GPIO, s_brightness);
    return ESP_OK;
}

void neopixel_set_state(led_state_t state)
{
    s_state = state;
}

void neopixel_set_brightness(uint8_t percent)
{
    if (percent < 1) percent = 1;
    if (percent > 100) percent = 100;
    s_brightness = percent;
}

uint8_t neopixel_get_brightness(void)
{
    return s_brightness;
}

led_state_t neopixel_get_state(void)
{
    return s_state;
}
