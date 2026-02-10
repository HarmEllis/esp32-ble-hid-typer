#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LED_STATE_OFF,
    LED_STATE_PROVISIONING,     /* Orange slow blink (1s on/off) */
    LED_STATE_BLE_CONNECTED,    /* Solid blue */
    LED_STATE_WIFI_CONNECTED,   /* Solid white (Phase 3) */
    LED_STATE_WSS_CONNECTED,    /* Solid yellow (Phase 3) */
    LED_STATE_TYPING,           /* Red flash (500ms on/off) */
    LED_STATE_RESET_WARNING,    /* Yellow rapid flash (100ms on/off) */
    LED_STATE_RESET_CONFIRMED,  /* Solid red (1s) */
    LED_STATE_ERROR,            /* Red rapid blink (100ms on/off) */
    LED_STATE_OTA,              /* Purple pulsing (Phase 3) */
} led_state_t;

esp_err_t neopixel_init(void);
void neopixel_set_state(led_state_t state);
void neopixel_set_brightness(uint8_t percent);
uint8_t neopixel_get_brightness(void);
led_state_t neopixel_get_state(void);
void neopixel_set_typing_indicator(bool enabled);
void neopixel_set_typing_key_down(bool key_down);
