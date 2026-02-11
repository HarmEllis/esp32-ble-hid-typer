# ESP32 BLE HID Typer

## Project Overview

Monorepo with ESP32 firmware and a Preact PWA.

Current implementation is BLE-first:
- ESP32-S3 exposes a USB HID keyboard.
- PWA connects over Web Bluetooth for provisioning and typing control.
- PWA includes Web Serial flashing (`esptool-js`).

Phase 3 features (WiFi manager, HTTPS/WSS transport, certificate flow, OTA over network) are planned but not implemented.

**Supported hardware now**: ESP32-S3 only. BLE is required.

All code, comments, UI, and docs are English.

## Current Status

| Area | Status |
|---|---|
| Phase 1: Dev environment + CI/CD | Implemented |
| Phase 2: BLE provisioning + BLE typing flow | Implemented (with a few partial items) |
| Phase 3: WiFi/WSS/cert/OTA transport | Planned |

## Architecture (Current)

```
┌──────────────────┐      BLE GATT       ┌─────────────────┐     USB HID      ┌──────────────┐
│ Preact PWA       │  ───────────────►   │   ESP32-S3      │  ─────────────►  │ Target PC    │
│ (GitHub Pages)   │                     │                 │    Keystrokes    │              │
│ - Web Bluetooth  │                     │ - USB HID       │                  └──────────────┘
│ - Web Serial     │                     │ - BLE server    │
└──────────────────┘                     │ - Provisioning  │
                                         │ - Auth/rate lim │
                                         │ - Audit log     │
                                         │ - NeoPixel LED  │
                                         └─────────────────┘
```

## Repository Structure (Actual)

```
esp32-ble-hid-typer/
├── .devcontainer/
├── .github/
│   ├── workflows/
│   │   ├── build-firmware.yml
│   │   └── deploy-webapp.yml
│   └── scripts/
│       └── prepare-pages-firmware.mjs
├── firmware/
│   ├── CMakeLists.txt
│   ├── partitions.csv
│   ├── sdkconfig.defaults*
│   └── main/
│       ├── main.c
│       ├── provisioning.*
│       ├── ble_server.*
│       ├── ble_security.*
│       ├── auth.*
│       ├── usb_hid.*
│       ├── typing_engine.*
│       ├── neopixel.*
│       ├── button_reset.*
│       ├── serial_cmd.*
│       ├── audit_log.*
│       ├── nvs_storage.*
│       └── keymap_us.h
├── webapp/
│   ├── package.json
│   ├── vite.config.ts
│   └── src/
│       ├── app.tsx
│       ├── components/
│       │   ├── ConnectionScreen.tsx
│       │   ├── ProvisioningScreen.tsx
│       │   ├── BleConnect.tsx
│       │   ├── TextSender.tsx
│       │   ├── Settings.tsx
│       │   ├── AuditLog.tsx
│       │   ├── FirmwareFlash.tsx
│       │   ├── PinSetup.tsx
│       │   ├── StatusBar.tsx
│       │   ├── ClipboardPaste.tsx
│       │   └── VirtualKeyboard.tsx
│       ├── types/protocol.ts
│       └── utils/
└── docs/
    └── OTA_SIGNING.md
```

### Planned Phase 3 Modules (Not Yet in Tree)

`wifi_manager.*`, `https_server.*`, `http_server.*`, `ws_handler.*`, `tls_certs.*`, `ota.*`, `ble_cert_service.*`, and matching webapp network/certificate/OTA screens.

## BLE Protocol (Normal Mode)

**Service UUID**: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`

**Characteristics**:
1. Text Input `6e400002` (Write / Write No Response)
2. Status `6e400003` (Read, Notify)
3. PIN Management `6e400004` (Write)
4. WiFi Config `6e400005` (Write, Read) - stub returns `{"error":"not_available"}`
5. Cert Fingerprint `6e400006` (Read) - stub returns 64 zeroes

**Status JSON fields** (actual):
- `connected`
- `typing`
- `queue`
- `authenticated`
- `keyboard_connected`
- `retry_delay_ms`
- `locked_out`
- optional `auth_error` (`invalid_pin`, `rate_limited`, `locked_out`)

**PIN Management actions** (actual):
- `{"action":"auth","pin":"123456"}`
- `{"action":"verify","pin":"123456"}`
- `{"action":"logout"}`
- `{"action":"set","old":"123456","new":"654321"}`
- `{"action":"set_config","key":"typing_delay|led_brightness","value":"..."}`
- `{"action":"get_logs"}`
- `{"action":"abort"}`
- `{"action":"key_combo","modifier":<0-255>,"keycode":<0-255>}`

## BLE Provisioning Protocol

**Device name**: `ESP32-HID-SETUP`

**Service UUID**: `00467768-6228-2272-4663-277478268000`

**Characteristics**:
- Status `...8001` (Read, Notify)
- Error `...8002` (Read, Notify)
- RPC Command `...8003` (Write)
- RPC Result `...8004` (Read, Notify)

**RPC commands**:
- `{"command":"set_pin","pin":"123456"}`
- `{"command":"set_wifi","ssid":"MyNet","password":"pass"}` (credentials saved only)
- `{"command":"complete"}`

Webapp currently uses `set_pin` + `complete`; no WiFi input screen yet.

## Security Model (Current)

- App-layer PIN auth is required before typing/config/log actions in normal mode.
- Rate limiting/backoff/lockout is enforced in firmware (`auth.c`).
- NVS encryption is used when `nvs_keys` partition is available; firmware falls back to unencrypted NVS if keys partition is missing.
- BLE link-layer security is currently disabled in normal mode (`ble_hs_cfg.sm_sc = 0`, `sm_bonding = 0`, `sm_mitm = 0`). Current protection relies on app-layer PIN auth.

## LED Behavior (Current)

- Provisioning: orange slow blink (1s)
- Normal disconnected: off
- BLE connected: blue solid
- Reset warning (BOOT 2-10s): yellow fast blink
- Reset confirmed (10s+): red solid
- Typing indicator currently uses key-timed blink (orange on key down, off on key up)
- `LED_STATE_TYPING` (red 500ms flash) exists in `neopixel.c` but is not used by the active typing path (`typing_engine.c` uses `neopixel_set_typing_indicator` and `neopixel_set_typing_key_down`).

`LED_STATE_WIFI_CONNECTED`, `LED_STATE_WSS_CONNECTED`, and OTA-specific display states exist in enum but are Phase 3 scope.

## Factory Reset + Serial

**BOOT button** (`GPIO0`): hold 10s to factory reset (`credentials`, `auth`, `config` namespaces erased).

**Serial commands (implemented)**:
- `status`
- `heap`
- `factory_reset`
- `full_reset`
- `reboot`
- `help`

`full_reset` wipes all known NVS namespaces (`credentials`, `auth`, `config`, `audit`, `certs`). Certificate generation/verification flow is not implemented yet.

## Build and CI/CD

### Local build commands

```bash
# Firmware
cd firmware && idf.py set-target esp32s3 && idf.py build

# Webapp dev
cd webapp && npm run dev

# Webapp prod
cd webapp && npm run build
```

### GitHub Actions

- `build-firmware.yml`
  - Trigger: tag push `v*` or manual dispatch
  - Builds in `espressif/idf:v5.3`
  - Signs with `OTA_SIGNING_KEY`
  - Publishes `firmware-esp32s3.bin`, `firmware-esp32s3.sig`, `bootloader-esp32s3.bin`, `partition-table-esp32s3.bin`

- `deploy-webapp.yml`
  - Trigger: successful `Build Firmware` workflow run (push event) or manual dispatch with tag
  - Uses Node 24
  - Runs `.github/scripts/prepare-pages-firmware.mjs` to mirror release firmware files into `webapp/public/firmware`
  - Builds and deploys GitHub Pages

## Planned Phase 3 (Not Implemented)

- WiFi manager and BLE WiFi actions
- Certificate generation + fingerprint flow + HTTP cert endpoint
- HTTPS/WSS transport and network UI
- OTA over network with signature verification path
- Transport manager (BLE/WSS switching)
- Dedicated firmware SysRq command/audit pipeline (current UI uses generic `key_combo` only)

For detailed truth tables and implementation tracking, see `FEATURES.md` and `PLAN.md`.
