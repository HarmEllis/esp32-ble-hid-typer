#include "typing_engine.h"
#include "usb_hid.h"
#include "keymap_us.h"
#include "neopixel.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "typing_engine";

#define DEFAULT_DELAY_MS    10
#define MIN_DELAY_MS        5
#define MAX_DELAY_MS        100
#define KEY_PRESS_HOLD_MS   2

static char s_queue[TYPING_QUEUE_MAX_SIZE];
static volatile uint32_t s_queue_head;
static volatile uint32_t s_queue_tail;
static volatile uint32_t s_queue_total;
static volatile uint32_t s_queue_typed;
static volatile bool s_abort;
static volatile bool s_typing;
static uint16_t s_delay_ms = DEFAULT_DELAY_MS;
static typing_progress_cb_t s_progress_cb;
static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_task_handle;
static led_state_t s_prev_led_state;

static uint32_t queue_used(void)
{
    if (s_queue_tail >= s_queue_head) {
        return s_queue_tail - s_queue_head;
    }
    return TYPING_QUEUE_MAX_SIZE - s_queue_head + s_queue_tail;
}

static bool queue_pop(char *ch)
{
    if (s_queue_head == s_queue_tail) return false;
    *ch = s_queue[s_queue_head];
    s_queue_head = (s_queue_head + 1) % TYPING_QUEUE_MAX_SIZE;
    return true;
}

static void type_char(char ch)
{
    if ((uint8_t)ch >= 128) return;  /* Skip non-ASCII */

    const hid_keymap_entry_t *entry = &KEYMAP_US[(uint8_t)ch];
    if (entry->keycode == 0x00 && ch != 0) return;  /* Unmapped character */

    usb_hid_send_key(entry->modifier, entry->keycode);
    vTaskDelay(pdMS_TO_TICKS(KEY_PRESS_HOLD_MS));
    usb_hid_release_keys();
}

static void typing_task(void *arg)
{
    char ch;

    while (1) {
        /* Wait for data in queue */
        while (queue_used() == 0 || s_abort) {
            if (s_typing) {
                s_typing = false;
                neopixel_set_state(s_prev_led_state);
            }
            if (s_abort) {
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                s_queue_head = 0;
                s_queue_tail = 0;
                s_queue_total = 0;
                s_queue_typed = 0;
                s_abort = false;
                xSemaphoreGive(s_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        /* Start typing */
        if (!s_typing) {
            s_typing = true;
            s_prev_led_state = neopixel_get_state();
            neopixel_set_state(LED_STATE_TYPING);
        }

        if (queue_pop(&ch)) {
            type_char(ch);
            s_queue_typed++;

            if (s_progress_cb) {
                s_progress_cb(s_queue_typed, s_queue_total);
            }

            vTaskDelay(pdMS_TO_TICKS(s_delay_ms));
        }
    }
}

esp_err_t typing_engine_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) return ESP_ERR_NO_MEM;

    s_queue_head = 0;
    s_queue_tail = 0;
    s_queue_total = 0;
    s_queue_typed = 0;
    s_abort = false;
    s_typing = false;

    xTaskCreate(typing_task, "typing", 4096, NULL, 4, &s_task_handle);
    ESP_LOGI(TAG, "Typing engine initialized (delay=%dms)", s_delay_ms);
    return ESP_OK;
}

esp_err_t typing_engine_enqueue(const char *text, size_t len)
{
    if (len == 0) return ESP_OK;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    uint32_t free_space = TYPING_QUEUE_MAX_SIZE - queue_used() - 1;
    if (len > free_space) {
        xSemaphoreGive(s_mutex);
        ESP_LOGW(TAG, "Queue full: need %u, have %lu", (unsigned)len, (unsigned long)free_space);
        return ESP_ERR_NO_MEM;
    }

    /* Reset progress counters for new batch */
    if (queue_used() == 0) {
        s_queue_total = len;
        s_queue_typed = 0;
    } else {
        s_queue_total += len;
    }

    for (size_t i = 0; i < len; i++) {
        s_queue[s_queue_tail] = text[i];
        s_queue_tail = (s_queue_tail + 1) % TYPING_QUEUE_MAX_SIZE;
    }

    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Enqueued %u chars (total in queue: %lu)", (unsigned)len, (unsigned long)queue_used());
    return ESP_OK;
}

void typing_engine_abort(void)
{
    s_abort = true;
    ESP_LOGI(TAG, "Abort requested");
}

void typing_engine_set_delay_ms(uint16_t delay_ms)
{
    if (delay_ms < MIN_DELAY_MS) delay_ms = MIN_DELAY_MS;
    if (delay_ms > MAX_DELAY_MS) delay_ms = MAX_DELAY_MS;
    s_delay_ms = delay_ms;
    ESP_LOGI(TAG, "Typing delay set to %d ms", s_delay_ms);
}

uint16_t typing_engine_get_delay_ms(void)
{
    return s_delay_ms;
}

void typing_engine_set_progress_callback(typing_progress_cb_t cb)
{
    s_progress_cb = cb;
}

bool typing_engine_is_typing(void)
{
    return s_typing;
}

uint32_t typing_engine_queue_length(void)
{
    return queue_used();
}
