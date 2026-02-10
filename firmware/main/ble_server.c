#include "ble_server.h"
#include "ble_security.h"
#include "typing_engine.h"
#include "auth.h"
#include "audit_log.h"
#include "neopixel.h"
#include "nvs_storage.h"
#include "usb_hid.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "cJSON.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "ble_server";

#define DEVICE_NAME "ESP32-HID-Typer"

static uint8_t s_own_addr_type;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_text_input_val_handle;
static uint16_t s_status_val_handle;
static uint16_t s_pin_mgmt_val_handle;
static uint16_t s_wifi_config_val_handle;
static uint16_t s_cert_fp_val_handle;
static bool s_authenticated;

typedef enum {
    AUTH_ERROR_NONE = 0,
    AUTH_ERROR_INVALID_PIN,
    AUTH_ERROR_RATE_LIMITED,
    AUTH_ERROR_LOCKED_OUT,
} auth_error_state_t;

static auth_error_state_t s_auth_error = AUTH_ERROR_NONE;

/* Service UUID: 6e400001-b5a3-f393-e0a9-e50e24dcca9e (little-endian) */
static const ble_uuid128_t svc_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

/* Text Input: 6e400002-... */
static const ble_uuid128_t text_input_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

/* Status: 6e400003-... */
static const ble_uuid128_t status_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

/* PIN Management: 6e400004-... */
static const ble_uuid128_t pin_mgmt_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x04, 0x00, 0x40, 0x6e);

/* WiFi Config: 6e400005-... (stub) */
static const ble_uuid128_t wifi_config_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x05, 0x00, 0x40, 0x6e);

/* Cert Fingerprint: 6e400006-... (stub) */
static const ble_uuid128_t cert_fp_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x06, 0x00, 0x40, 0x6e);

/* Forward declarations */
static int text_input_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);
static int status_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg);
static int pin_mgmt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);
static int wifi_config_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);
static int cert_fp_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
static void notify_status_if_connected(void);

static esp_err_t send_key_combo(uint8_t modifier, uint8_t keycode)
{
    if (!usb_hid_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = usb_hid_send_key(modifier, keycode);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(6));
    return usb_hid_release_keys();
}

static void reset_session_auth(void)
{
    s_authenticated = false;
    s_auth_error = AUTH_ERROR_NONE;
}

static void set_session_auth_result(auth_result_t result)
{
    switch (result) {
    case AUTH_OK:
        s_authenticated = true;
        s_auth_error = AUTH_ERROR_NONE;
        break;
    case AUTH_FAIL_INVALID_PIN:
        s_authenticated = false;
        s_auth_error = AUTH_ERROR_INVALID_PIN;
        break;
    case AUTH_FAIL_RATE_LIMITED:
        s_authenticated = false;
        s_auth_error = AUTH_ERROR_RATE_LIMITED;
        break;
    case AUTH_FAIL_LOCKED_OUT:
        s_authenticated = false;
        s_auth_error = AUTH_ERROR_LOCKED_OUT;
        break;
    default:
        s_authenticated = false;
        s_auth_error = AUTH_ERROR_INVALID_PIN;
        break;
    }
}

static const char *auth_error_to_string(auth_error_state_t state)
{
    switch (state) {
    case AUTH_ERROR_INVALID_PIN:
        return "invalid_pin";
    case AUTH_ERROR_RATE_LIMITED:
        return "rate_limited";
    case AUTH_ERROR_LOCKED_OUT:
        return "locked_out";
    case AUTH_ERROR_NONE:
    default:
        return NULL;
    }
}

/* GATT service definition */
static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /* Text Input (Write) */
                .uuid = &text_input_uuid.u,
                .access_cb = text_input_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &s_text_input_val_handle,
            },
            {
                /* Status (Read, Notify) */
                .uuid = &status_uuid.u,
                .access_cb = status_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_status_val_handle,
            },
            {
                /* PIN Management (Write) */
                .uuid = &pin_mgmt_uuid.u,
                .access_cb = pin_mgmt_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &s_pin_mgmt_val_handle,
            },
            {
                /* WiFi Config (stub) */
                .uuid = &wifi_config_uuid.u,
                .access_cb = wifi_config_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                .val_handle = &s_wifi_config_val_handle,
            },
            {
                /* Cert Fingerprint (stub) */
                .uuid = &cert_fp_uuid.u,
                .access_cb = cert_fp_access_cb,
                .flags = BLE_GATT_CHR_F_READ,
                .val_handle = &s_cert_fp_val_handle,
            },
            { 0 },
        },
    },
    { 0 },
};

/* Text Input write */
static int text_input_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;
    if (!s_authenticated) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;

    uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
    if (om_len == 0) return 0;
    if (om_len > 512) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    char buf[513];
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, om_len, NULL);
    if (rc != 0) return BLE_ATT_ERR_UNLIKELY;
    buf[om_len] = '\0';

    ESP_LOGI(TAG, "Text input received (%d bytes)", om_len);
    typing_engine_enqueue(buf, om_len);
    return 0;
}

/* Status read */
static int status_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;

    const char *auth_error = auth_error_to_string(s_auth_error);
    uint32_t retry_delay_ms = s_authenticated ? 0 : auth_get_retry_delay_ms();
    bool locked_out = auth_is_locked_out();

    char json[256];
    int len;
    if (auth_error != NULL) {
        len = snprintf(json, sizeof(json),
                       "{\"connected\":true,\"typing\":%s,\"queue\":%lu,"
                       "\"authenticated\":%s,\"keyboard_connected\":%s,\"retry_delay_ms\":%lu,"
                       "\"locked_out\":%s,\"auth_error\":\"%s\"}",
                       typing_engine_is_typing() ? "true" : "false",
                       (unsigned long)typing_engine_queue_length(),
                       s_authenticated ? "true" : "false",
                       usb_hid_ready() ? "true" : "false",
                       (unsigned long)retry_delay_ms,
                       locked_out ? "true" : "false",
                       auth_error);
    } else {
        len = snprintf(json, sizeof(json),
                       "{\"connected\":true,\"typing\":%s,\"queue\":%lu,"
                       "\"authenticated\":%s,\"keyboard_connected\":%s,\"retry_delay_ms\":%lu,"
                       "\"locked_out\":%s}",
                       typing_engine_is_typing() ? "true" : "false",
                       (unsigned long)typing_engine_queue_length(),
                       s_authenticated ? "true" : "false",
                       usb_hid_ready() ? "true" : "false",
                       (unsigned long)retry_delay_ms,
                       locked_out ? "true" : "false");
    }

    if (len < 0 || len >= (int)sizeof(json)) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    int rc = os_mbuf_append(ctxt->om, json, len);
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

/* PIN Management write */
static int pin_mgmt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;

    uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
    if (om_len > 256) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    char buf[257];
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, om_len, NULL);
    if (rc != 0) return BLE_ATT_ERR_UNLIKELY;
    buf[om_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) return BLE_ATT_ERR_UNLIKELY;

    cJSON *action = cJSON_GetObjectItem(root, "action");
    if (!action || !cJSON_IsString(action)) {
        cJSON_Delete(root);
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (strcmp(action->valuestring, "auth") == 0 ||
        strcmp(action->valuestring, "verify") == 0) {
        cJSON *pin = cJSON_GetObjectItem(root, "pin");
        if (!pin || !cJSON_IsString(pin)) {
            cJSON_Delete(root);
            return BLE_ATT_ERR_UNLIKELY;
        }

        auth_result_t result = auth_verify_pin(pin->valuestring);
        set_session_auth_result(result);
        if (result == AUTH_OK) {
            audit_log_event(AUDIT_AUTH_ATTEMPT, "transport=ble result=success");
            ESP_LOGI(TAG, "BLE session authenticated");
        } else {
            audit_log_event(AUDIT_AUTH_ATTEMPT, "transport=ble result=fail");
            ESP_LOGW(TAG, "BLE session auth failed: result=%d", (int)result);
        }
        notify_status_if_connected();
    } else if (strcmp(action->valuestring, "logout") == 0) {
        reset_session_auth();
        notify_status_if_connected();
        ESP_LOGI(TAG, "BLE session logged out");
    } else if (strcmp(action->valuestring, "set") == 0) {
        if (!s_authenticated) {
            cJSON_Delete(root);
            return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        }

        cJSON *old_pin = cJSON_GetObjectItem(root, "old");
        cJSON *new_pin = cJSON_GetObjectItem(root, "new");

        if (old_pin && new_pin && cJSON_IsString(old_pin) && cJSON_IsString(new_pin)) {
            auth_result_t result = auth_set_pin(old_pin->valuestring, new_pin->valuestring);
            if (result == AUTH_OK) {
                /* Update BLE passkey */
                uint32_t new_passkey = (uint32_t)atoi(new_pin->valuestring);
                ble_security_set_passkey(new_passkey);
                audit_log_event(AUDIT_PIN_CHANGE, "transport=ble");
                ESP_LOGI(TAG, "PIN changed via BLE");
            } else {
                audit_log_event(AUDIT_AUTH_ATTEMPT, "transport=ble result=fail action=pin_change");
                ESP_LOGW(TAG, "PIN change failed: result=%d", (int)result);
            }
        }
    } else if (strcmp(action->valuestring, "set_config") == 0) {
        if (!s_authenticated) {
            cJSON_Delete(root);
            return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        }

        cJSON *key = cJSON_GetObjectItem(root, "key");
        cJSON *value = cJSON_GetObjectItem(root, "value");
        if (key && value && cJSON_IsString(key) && cJSON_IsString(value)) {
            int value_num = atoi(value->valuestring);
            if (strcmp(key->valuestring, "typing_delay") == 0) {
                typing_engine_set_delay_ms((uint16_t)value_num);
                nvs_storage_set_u16("config", "typing_delay", (uint16_t)value_num);
            } else if (strcmp(key->valuestring, "led_brightness") == 0) {
                neopixel_set_brightness((uint8_t)value_num);
                nvs_storage_set_u8("config", "led_brightness", (uint8_t)value_num);
            }
        } else {
            cJSON *delay = cJSON_GetObjectItem(root, "typing_delay");
            if (delay && cJSON_IsNumber(delay)) {
                typing_engine_set_delay_ms((uint16_t)delay->valueint);
                nvs_storage_set_u16("config", "typing_delay", (uint16_t)delay->valueint);
            }
            cJSON *brightness = cJSON_GetObjectItem(root, "led_brightness");
            if (brightness && cJSON_IsNumber(brightness)) {
                neopixel_set_brightness((uint8_t)brightness->valueint);
                nvs_storage_set_u8("config", "led_brightness", (uint8_t)brightness->valueint);
            }
        }
    } else if (strcmp(action->valuestring, "get_logs") == 0) {
        if (!s_authenticated) {
            cJSON_Delete(root);
            return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        }

        /* Send audit log via status notification */
        char log_buf[512];
        size_t log_len = audit_log_get_entries(log_buf, sizeof(log_buf));
        if (log_len > 0 && s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            struct os_mbuf *om = ble_hs_mbuf_from_flat(log_buf, log_len);
            if (om) {
                ble_gatts_notify_custom(s_conn_handle, s_status_val_handle, om);
            }
        }
    } else if (strcmp(action->valuestring, "abort") == 0) {
        if (!s_authenticated) {
            cJSON_Delete(root);
            return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        }
        typing_engine_abort();
    } else if (strcmp(action->valuestring, "key_combo") == 0) {
        if (!s_authenticated) {
            cJSON_Delete(root);
            return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
        }

        cJSON *modifier = cJSON_GetObjectItem(root, "modifier");
        cJSON *keycode = cJSON_GetObjectItem(root, "keycode");
        if (!modifier || !cJSON_IsNumber(modifier) || !keycode || !cJSON_IsNumber(keycode)) {
            cJSON_Delete(root);
            return BLE_ATT_ERR_UNLIKELY;
        }

        int mod = modifier->valueint;
        int key = keycode->valueint;
        if (mod < 0 || mod > 255 || key < 0 || key > 255) {
            cJSON_Delete(root);
            return BLE_ATT_ERR_UNLIKELY;
        }

        esp_err_t combo_err = send_key_combo((uint8_t)mod, (uint8_t)key);
        if (combo_err != ESP_OK) {
            cJSON_Delete(root);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    cJSON_Delete(root);
    return 0;
}

/* WiFi Config (stub) */
static int wifi_config_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        const char *msg = "{\"error\":\"not_available\"}";
        int rc = os_mbuf_append(ctxt->om, msg, strlen(msg));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return 0;
}

/* Cert Fingerprint (stub) */
static int cert_fp_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        const char *placeholder = "0000000000000000000000000000000000000000000000000000000000000000";
        int rc = os_mbuf_append(ctxt->om, placeholder, 64);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return 0;
}

/* Typing progress callback — called from typing engine task */
static void on_typing_progress(uint32_t current, uint32_t total)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) return;

    char json[64];
    int len = snprintf(json, sizeof(json),
                       "{\"typing\":true,\"current\":%lu,\"total\":%lu}",
                       (unsigned long)current, (unsigned long)total);

    struct os_mbuf *om = ble_hs_mbuf_from_flat(json, len);
    if (om) {
        ble_gatts_notify_custom(s_conn_handle, s_status_val_handle, om);
    }

    /* Notify when typing is complete */
    if (current >= total) {
        neopixel_set_state(LED_STATE_BLE_CONNECTED);
    }
}

/* GAP event handler */
static int gap_event_handler(struct ble_gap_event *event, void *arg);
static void start_advertising(void);

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    /* Delegate security events */
    int sec_rc = ble_security_gap_event(event, arg);
    if (event->type == BLE_GAP_EVENT_PASSKEY_ACTION ||
        event->type == BLE_GAP_EVENT_REPEAT_PAIRING ||
        event->type == BLE_GAP_EVENT_ENC_CHANGE) {
        return sec_rc;
    }

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            reset_session_auth();
            neopixel_set_state(LED_STATE_BLE_CONNECTED);
            audit_log_event(AUDIT_BLE_CONNECT, NULL);
            ESP_LOGI(TAG, "BLE connected (handle=%d)", s_conn_handle);
        } else {
            ESP_LOGW(TAG, "BLE connection failed: status=%d", event->connect.status);
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnected (reason=%d)", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        reset_session_auth();
        neopixel_set_state(LED_STATE_OFF);
        audit_log_event(AUDIT_BLE_DISCONNECT, NULL);
        start_advertising();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU updated: conn=%d, mtu=%d",
                 event->mtu.conn_handle, event->mtu.value);
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Subscribe event: handle=%d, cur_notify=%d",
                 event->subscribe.attr_handle, event->subscribe.cur_notify);
        break;

    default:
        break;
    }
    return 0;
}

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting adv fields: rc=%d", rc);
        return;
    }

    /* Set scan response with service UUID */
    struct ble_hs_adv_fields rsp_fields = {0};
    rsp_fields.uuids128 = (ble_uuid128_t[]) { svc_uuid };
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting scan response: rc=%d", rc);
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertising: rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising as \"%s\"", DEVICE_NAME);
    }
}

static void on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type: rc=%d", rc);
        return;
    }
    start_advertising();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset: reason=%d", reason);
}

static void nimble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_server_init(void)
{
    ESP_LOGI(TAG, "Starting BLE server (normal mode)");

    /* Initialize NimBLE */
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Set device name */
    ble_svc_gap_device_name_set(DEVICE_NAME);

    /* Initialize security */
    ble_security_init();
    reset_session_auth();

    /* Initialize GATT services */
    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: rc=%d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: rc=%d", rc);
        return ESP_FAIL;
    }

    /* Configure host callbacks */
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Register typing progress callback */
    typing_engine_set_progress_callback(on_typing_progress);

    /* Load saved config */
    uint16_t delay = 0;
    if (nvs_storage_get_u16("config", "typing_delay", &delay) == ESP_OK && delay > 0) {
        typing_engine_set_delay_ms(delay);
    }
    uint8_t brightness = 0;
    if (nvs_storage_get_u8("config", "led_brightness", &brightness) == ESP_OK && brightness > 0) {
        neopixel_set_brightness(brightness);
    }

    /* Start NimBLE host task */
    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "BLE server initialized");
    return ESP_OK;
}

void ble_server_stop(void)
{
    int rc = nimble_port_stop();
    if (rc == 0) {
        nimble_port_deinit();
        ESP_LOGI(TAG, "BLE server stopped");
    }
}

bool ble_server_is_connected(void)
{
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

static void notify_status_if_connected(void)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) return;
    ble_gatts_chr_updated(s_status_val_handle);
}

void ble_server_notify_status(void)
{
    notify_status_if_connected();
}

void ble_server_notify_progress(uint32_t current, uint32_t total)
{
    on_typing_progress(current, total);
}
