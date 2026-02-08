#include "provisioning.h"
#include "nvs_storage.h"
#include "neopixel.h"
#include "auth.h"
#include "audit_log.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "provisioning";

#define DEVICE_NAME "ESP32-HID-SETUP"

static volatile bool s_active = true;
static uint8_t s_own_addr_type;
static uint16_t s_status_val_handle;
static uint16_t s_error_val_handle;
static uint16_t s_rpc_result_val_handle;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;

/* Provisioning status values */
static uint8_t s_prov_status = 0;  /* 0=ready, 1=provisioning, 2=provisioned */
static uint8_t s_prov_error = 0;   /* 0=none, 1=invalid_pin, 2=unable_to_connect, 3=unknown */

/* Improv WiFi-compatible UUIDs (little-endian) */
static const ble_uuid128_t prov_svc_uuid =
    BLE_UUID128_INIT(0x00, 0x80, 0x26, 0x78, 0x74, 0x27, 0x63, 0x46,
                     0x72, 0x22, 0x28, 0x62, 0x68, 0x77, 0x46, 0x00);

static const ble_uuid128_t prov_status_chr_uuid =
    BLE_UUID128_INIT(0x01, 0x80, 0x26, 0x78, 0x74, 0x27, 0x63, 0x46,
                     0x72, 0x22, 0x28, 0x62, 0x68, 0x77, 0x46, 0x00);

static const ble_uuid128_t prov_error_chr_uuid =
    BLE_UUID128_INIT(0x02, 0x80, 0x26, 0x78, 0x74, 0x27, 0x63, 0x46,
                     0x72, 0x22, 0x28, 0x62, 0x68, 0x77, 0x46, 0x00);

static const ble_uuid128_t prov_rpc_cmd_chr_uuid =
    BLE_UUID128_INIT(0x03, 0x80, 0x26, 0x78, 0x74, 0x27, 0x63, 0x46,
                     0x72, 0x22, 0x28, 0x62, 0x68, 0x77, 0x46, 0x00);

static const ble_uuid128_t prov_rpc_result_chr_uuid =
    BLE_UUID128_INIT(0x04, 0x80, 0x26, 0x78, 0x74, 0x27, 0x63, 0x46,
                     0x72, 0x22, 0x28, 0x62, 0x68, 0x77, 0x46, 0x00);

/* Send JSON response via RPC Result notification */
static void send_rpc_response(const char *json_str)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) return;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(json_str, strlen(json_str));
    if (om) {
        ble_gatts_notify_custom(s_conn_handle, s_rpc_result_val_handle, om);
    }
}

static void update_status(uint8_t status)
{
    s_prov_status = status;
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) return;
    ble_gatts_chr_updated(s_status_val_handle);
}

static void update_error(uint8_t error)
{
    s_prov_error = error;
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) return;
    ble_gatts_chr_updated(s_error_val_handle);
}

/* Handle set_pin command */
static void handle_set_pin(cJSON *root)
{
    cJSON *pin_json = cJSON_GetObjectItem(root, "pin");
    if (!pin_json || !cJSON_IsString(pin_json)) {
        update_error(1);
        send_rpc_response("{\"success\":false,\"message\":\"Missing pin field\"}");
        return;
    }

    const char *pin = pin_json->valuestring;
    if (!auth_validate_pin_format(pin)) {
        update_error(1);
        send_rpc_response("{\"success\":false,\"message\":\"Invalid PIN format\"}");
        return;
    }

    esp_err_t err = nvs_storage_set_pin(pin);
    if (err != ESP_OK) {
        update_error(3);
        send_rpc_response("{\"success\":false,\"message\":\"Failed to store PIN\"}");
        return;
    }

    update_status(1);  /* provisioning in progress */
    update_error(0);
    send_rpc_response("{\"success\":true,\"message\":\"PIN set successfully\"}");
    ESP_LOGI(TAG, "PIN set via provisioning");
}

/* Handle set_wifi command */
static void handle_set_wifi(cJSON *root)
{
    cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass_json = cJSON_GetObjectItem(root, "password");

    if (!ssid_json || !cJSON_IsString(ssid_json)) {
        update_error(3);
        send_rpc_response("{\"success\":false,\"message\":\"Missing ssid field\"}");
        return;
    }

    nvs_storage_set_str("credentials", "wifi_ssid", ssid_json->valuestring);
    if (pass_json && cJSON_IsString(pass_json)) {
        nvs_storage_set_str("credentials", "wifi_pass", pass_json->valuestring);
    }

    /* In Phase 2, we don't attempt WiFi connection */
    update_error(0);
    send_rpc_response("{\"success\":true,\"message\":\"WiFi credentials saved\"}");
    ESP_LOGI(TAG, "WiFi credentials stored via provisioning");
}

/* Handle complete command */
static void handle_complete(void)
{
    if (!nvs_storage_has_pin()) {
        update_error(1);
        send_rpc_response("{\"success\":false,\"message\":\"PIN must be set first\"}");
        return;
    }

    update_status(2);  /* provisioned */
    send_rpc_response("{\"success\":true,\"message\":\"Provisioning complete, rebooting...\"}");
    audit_log_event(AUDIT_BOOT, "provisioning_complete");
    audit_log_persist();

    ESP_LOGI(TAG, "Provisioning complete, rebooting in 1 second...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    s_active = false;
    esp_restart();
}

/* RPC Command characteristic write callback */
static int rpc_cmd_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    /* Read data from mbuf */
    uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
    if (om_len > 512) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    char buf[513];
    uint16_t copy_len = om_len;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, copy_len, NULL);
    if (rc != 0) return BLE_ATT_ERR_UNLIKELY;
    buf[copy_len] = '\0';

    ESP_LOGI(TAG, "RPC command: %s", buf);

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        send_rpc_response("{\"success\":false,\"message\":\"Invalid JSON\"}");
        return 0;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "command");
    if (!cmd || !cJSON_IsString(cmd)) {
        cJSON_Delete(root);
        send_rpc_response("{\"success\":false,\"message\":\"Missing command field\"}");
        return 0;
    }

    if (strcmp(cmd->valuestring, "set_pin") == 0) {
        handle_set_pin(root);
    } else if (strcmp(cmd->valuestring, "set_wifi") == 0) {
        handle_set_wifi(root);
    } else if (strcmp(cmd->valuestring, "complete") == 0) {
        cJSON_Delete(root);
        handle_complete();
        return 0;
    } else {
        send_rpc_response("{\"success\":false,\"message\":\"Unknown command\"}");
    }

    cJSON_Delete(root);
    return 0;
}

/* Status characteristic read callback */
static int status_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        int rc = os_mbuf_append(ctxt->om, &s_prov_status, sizeof(s_prov_status));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* Error characteristic read callback */
static int error_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        int rc = os_mbuf_append(ctxt->om, &s_prov_error, sizeof(s_prov_error));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* RPC Result characteristic read callback */
static int rpc_result_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        /* Return empty — results are sent via notifications */
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* GATT service definition */
static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &prov_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &prov_status_chr_uuid.u,
                .access_cb = status_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_status_val_handle,
            },
            {
                .uuid = &prov_error_chr_uuid.u,
                .access_cb = error_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_error_val_handle,
            },
            {
                .uuid = &prov_rpc_cmd_chr_uuid.u,
                .access_cb = rpc_cmd_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {
                .uuid = &prov_rpc_result_chr_uuid.u,
                .access_cb = rpc_result_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_rpc_result_val_handle,
            },
            { 0 },
        },
    },
    { 0 },
};

/* GAP event handler */
static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "Provisioning BLE connected (handle=%d)", s_conn_handle);
        } else {
            ESP_LOGW(TAG, "Provisioning BLE connection failed: %d", event->connect.status);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Provisioning BLE disconnected");
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        /* Restart advertising */
        if (s_active) {
            struct ble_gap_adv_params adv_params = {0};
            adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
            adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
            ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                              gap_event_handler, NULL);
        }
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (s_active) {
            struct ble_gap_adv_params adv_params = {0};
            adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
            adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
            ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                              gap_event_handler, NULL);
        }
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

esp_err_t provisioning_start(void)
{
    ESP_LOGI(TAG, "Starting provisioning mode");
    neopixel_set_state(LED_STATE_PROVISIONING);

    /* Initialize NimBLE */
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Set device name */
    ble_svc_gap_device_name_set(DEVICE_NAME);

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

    /* Configure host */
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    /* No security in provisioning mode */
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;

    /* Start NimBLE host task */
    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "Provisioning mode active - waiting for setup via PWA");
    return ESP_OK;
}

bool provisioning_is_active(void)
{
    return s_active;
}
