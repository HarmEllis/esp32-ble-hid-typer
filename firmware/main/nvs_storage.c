#include "nvs_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "nvs_storage";

#define NS_CREDENTIALS  "credentials"
#define NS_CONFIG       "config"
#define NS_AUTH         "auth"
#define NS_AUDIT        "audit"
#define NS_CERTS        "certs"

esp_err_t nvs_storage_init(void)
{
    /* Find the NVS keys partition for encryption */
    const esp_partition_t *keys_part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS,
        NULL);

    if (keys_part == NULL) {
        ESP_LOGW(TAG, "NVS keys partition not found, using unencrypted NVS");
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }
        return err;
    }

    /* Read or generate encryption keys */
    nvs_sec_cfg_t cfg;
    esp_err_t err = nvs_flash_read_security_cfg(keys_part, &cfg);
    if (err == ESP_ERR_NVS_KEYS_NOT_INITIALIZED) {
        ESP_LOGI(TAG, "Generating NVS encryption keys (first boot)");
        err = nvs_flash_generate_keys(keys_part, &cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to generate NVS keys: %s", esp_err_to_name(err));
            return err;
        }
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read NVS keys: %s", esp_err_to_name(err));
        return err;
    }

    /* Initialize encrypted NVS */
    err = nvs_flash_secure_init(&cfg);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS and retrying");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_secure_init(&cfg);
    }

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Encrypted NVS initialized");
    }
    return err;
}

bool nvs_storage_has_pin(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NS_CREDENTIALS, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    size_t len = 0;
    err = nvs_get_str(handle, "pin", NULL, &len);
    nvs_close(handle);
    return (err == ESP_OK && len > 1);
}

esp_err_t nvs_storage_get_pin(char *pin, size_t len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NS_CREDENTIALS, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    err = nvs_get_str(handle, "pin", pin, &len);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_set_pin(const char *pin)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NS_CREDENTIALS, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, "pin", pin);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

/* Generic accessors */

esp_err_t nvs_storage_get_u8(const char *ns, const char *key, uint8_t *val)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_u8(handle, key, val);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_set_u8(const char *ns, const char *key, uint8_t val)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(handle, key, val);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_get_u16(const char *ns, const char *key, uint16_t *val)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_u16(handle, key, val);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_set_u16(const char *ns, const char *key, uint16_t val)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_u16(handle, key, val);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_get_i64(const char *ns, const char *key, int64_t *val)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_i64(handle, key, val);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_set_i64(const char *ns, const char *key, int64_t val)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_i64(handle, key, val);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_get_str(const char *ns, const char *key, char *buf, size_t *len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_str(handle, key, buf, len);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_set_str(const char *ns, const char *key, const char *val)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_str(handle, key, val);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_get_blob(const char *ns, const char *key, void *buf, size_t *len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_blob(handle, key, buf, len);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_set_blob(const char *ns, const char *key, const void *data, size_t len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(handle, key, data, len);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_erase_key(const char *ns, const char *key)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_erase_key(handle, key);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_erase_namespace(const char *ns)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_erase_all(handle);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_storage_factory_reset(void)
{
    ESP_LOGW(TAG, "Factory reset: erasing credentials and auth");
    nvs_storage_erase_namespace(NS_CREDENTIALS);
    nvs_storage_erase_namespace(NS_AUTH);
    nvs_storage_erase_namespace(NS_CONFIG);
    return ESP_OK;
}

esp_err_t nvs_storage_full_reset(void)
{
    ESP_LOGW(TAG, "Full reset: erasing all NVS namespaces");
    nvs_storage_erase_namespace(NS_CREDENTIALS);
    nvs_storage_erase_namespace(NS_AUTH);
    nvs_storage_erase_namespace(NS_CONFIG);
    nvs_storage_erase_namespace(NS_AUDIT);
    nvs_storage_erase_namespace(NS_CERTS);
    return ESP_OK;
}
