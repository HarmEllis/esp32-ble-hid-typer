#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TYPING_QUEUE_MAX_SIZE 8192

typedef void (*typing_progress_cb_t)(uint32_t current, uint32_t total);

esp_err_t typing_engine_init(void);
esp_err_t typing_engine_enqueue(const char *text, size_t len);
void typing_engine_abort(void);
void typing_engine_set_delay_ms(uint16_t delay_ms);
uint16_t typing_engine_get_delay_ms(void);
void typing_engine_set_progress_callback(typing_progress_cb_t cb);
bool typing_engine_is_typing(void);
uint32_t typing_engine_queue_length(void);
