#include "ble_security.h"
#include "nvs_storage.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_store.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ble_security";

static uint32_t s_passkey;

esp_err_t ble_security_init(void)
{
    /* Load PIN as passkey */
    char pin[7] = {0};
    size_t len = sizeof(pin);
    esp_err_t err = nvs_storage_get_pin(pin, len);
    if (err == ESP_OK) {
        s_passkey = (uint32_t)atoi(pin);
    } else {
        s_passkey = 123456;  /* Fallback should never happen */
        ESP_LOGW(TAG, "Could not load PIN for passkey");
    }

    /* Configure Security Manager */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ESP_LOGI(TAG, "BLE security initialized (SC+MITM, passkey=%06lu)", (unsigned long)s_passkey);
    return ESP_OK;
}

void ble_security_set_passkey(uint32_t passkey)
{
    s_passkey = passkey;
    ESP_LOGI(TAG, "Passkey updated");
}

int ble_security_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        struct ble_sm_io pkey = {0};

        if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
            pkey.action = BLE_SM_IOACT_DISP;
            pkey.passkey = s_passkey;
            int rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            if (rc != 0) {
                ESP_LOGE(TAG, "Error injecting passkey: rc=%d", rc);
            } else {
                ESP_LOGI(TAG, "Passkey displayed: %06lu", (unsigned long)s_passkey);
            }
        } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
            pkey.action = BLE_SM_IOACT_NUMCMP;
            pkey.numcmp_accept = 1;
            ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        }
        return 0;
    }

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        /* Delete old bond and accept new pairing (keep only most recent) */
        struct ble_gap_conn_desc desc;
        int rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
            ESP_LOGI(TAG, "Deleted old bonding for re-pairing");
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    case BLE_GAP_EVENT_ENC_CHANGE: {
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "Encryption enabled (conn=%d)", event->enc_change.conn_handle);
        } else {
            ESP_LOGW(TAG, "Encryption change failed: status=%d", event->enc_change.status);
        }
        return 0;
    }

    default:
        return 0;
    }
}
