#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

esp_err_t nvs_storage_init(void);

/* PIN helpers */
bool nvs_storage_has_pin(void);
esp_err_t nvs_storage_get_pin(char *pin, size_t len);
esp_err_t nvs_storage_set_pin(const char *pin);

/* Generic typed accessors */
esp_err_t nvs_storage_get_u8(const char *ns, const char *key, uint8_t *val);
esp_err_t nvs_storage_set_u8(const char *ns, const char *key, uint8_t val);
esp_err_t nvs_storage_get_u16(const char *ns, const char *key, uint16_t *val);
esp_err_t nvs_storage_set_u16(const char *ns, const char *key, uint16_t val);
esp_err_t nvs_storage_get_i64(const char *ns, const char *key, int64_t *val);
esp_err_t nvs_storage_set_i64(const char *ns, const char *key, int64_t val);
esp_err_t nvs_storage_get_str(const char *ns, const char *key, char *buf, size_t *len);
esp_err_t nvs_storage_set_str(const char *ns, const char *key, const char *val);
esp_err_t nvs_storage_get_blob(const char *ns, const char *key, void *buf, size_t *len);
esp_err_t nvs_storage_set_blob(const char *ns, const char *key, const void *data, size_t len);
esp_err_t nvs_storage_erase_key(const char *ns, const char *key);
esp_err_t nvs_storage_erase_namespace(const char *ns);

/* Reset operations */
esp_err_t nvs_storage_factory_reset(void);  /* Erases credentials + auth. Keeps certs. */
esp_err_t nvs_storage_full_reset(void);     /* Erases everything. */
