#include "usb_hid.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "usb_hid";

/* HID Report Descriptor for a standard keyboard */
static const uint8_t s_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
};

/* Device descriptor */
static const tusb_desc_device_t s_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A,   /* Espressif VID */
    .idProduct          = 0x8100,   /* Custom PID */
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

/* Configuration descriptor */
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t s_config_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(s_hid_report_descriptor),
                       0x81, 16, 10),
};

/* String descriptors */
static const char *s_string_descriptor[] = {
    "",                         /* 0: Language (default) */
    "ESP32-BLE-HID-Typer",     /* 1: Manufacturer */
    "ESP32-S3 HID Keyboard",   /* 2: Product */
    "",                         /* 3: Serial (use chip ID) */
    "HID Interface",            /* 4: HID Interface */
};

/* Required TinyUSB callbacks */

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen)
{
    (void)instance; (void)report_id; (void)report_type;
    (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance; (void)report_id; (void)report_type;
    (void)buffer; (void)bufsize;
}

esp_err_t usb_hid_init(void)
{
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &s_device_descriptor,
        .string_descriptor = s_string_descriptor,
        .string_descriptor_count = sizeof(s_string_descriptor) / sizeof(s_string_descriptor[0]),
        .external_phy = false,
        .configuration_descriptor = s_config_descriptor,
    };

    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB install failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "USB HID keyboard initialized");
    return ESP_OK;
}

bool usb_hid_ready(void)
{
    return tud_mounted() && tud_hid_ready();
}

esp_err_t usb_hid_send_key(uint8_t modifier, uint8_t keycode)
{
    if (!tud_mounted()) return ESP_ERR_INVALID_STATE;

    /* Wait for HID ready */
    int retries = 50;
    while (!tud_hid_ready() && retries-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (!tud_hid_ready()) return ESP_ERR_TIMEOUT;

    uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
    if (!tud_hid_keyboard_report(1, modifier, keycodes)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t usb_hid_release_keys(void)
{
    if (!tud_mounted()) return ESP_ERR_INVALID_STATE;

    int retries = 50;
    while (!tud_hid_ready() && retries-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (!tud_hid_ready()) return ESP_ERR_TIMEOUT;

    if (!tud_hid_keyboard_report(1, 0, NULL)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
