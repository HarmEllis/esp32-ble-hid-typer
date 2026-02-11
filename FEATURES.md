# ESP32 BLE HID Typer — Feature Overview (Code-Accurate)

This document describes what the repository actually implements today.

## Status Legend

- `Implemented`: Present in current code and wired end-to-end.
- `Partial`: Present but limited, stubbed, or missing full UX/validation.
- `Planned (Phase 3)`: Not implemented yet.

## 1. Firmware Capabilities

### 1.1 Boot, Modes, and Core Runtime

| Capability | Status | Notes |
|---|---|---|
| Boot initializes NVS, LED, audit log, button monitor, serial console | Implemented | In `main.c` startup sequence |
| Automatic mode switch by PIN presence | Implemented | No PIN -> provisioning mode; PIN present -> normal mode |
| ESP32-S3 target | Implemented | Build and USB setup are S3-focused |

### 1.2 USB HID + Typing Engine

| Capability | Status | Notes |
|---|---|---|
| TinyUSB HID keyboard device | Implemented | Standard keyboard descriptor |
| UTF-8 input path into typing queue | Implemented | Non-ASCII bytes are skipped currently |
| Queueing and async typing task | Implemented | 8KB ring queue (`TYPING_QUEUE_MAX_SIZE=8192`) |
| Abort typing | Implemented | BLE action `abort` |
| Typing delay configuration (5-100 ms) | Implemented | Runtime + NVS persistence via `set_config` |
| Progress callback/notification | Implemented | Sent on status notify characteristic |
| 1000 chars/min hard cap | Partial | Documented target; no explicit chars/min throttle in current typing loop |

### 1.3 Provisioning Mode (BLE)

| Capability | Status | Notes |
|---|---|---|
| Provisioning advertisement (`ESP32-HID-SETUP`) | Implemented | Dedicated provisioning service |
| Improv-compatible UUID layout | Implemented | Service and char UUIDs match code constants |
| `set_pin` command | Implemented | Uses same PIN format rules as auth module |
| `set_wifi` command | Implemented | Stores SSID/password only; no connection attempt |
| `complete` command + reboot | Implemented | Requires PIN set |
| Provisioning security | Implemented | No BLE pairing in provisioning mode by design |

### 1.4 Normal Mode BLE Service

| Characteristic | UUID | Status | Notes |
|---|---|---|---|
| Text Input | `6e400002` | Implemented | Requires authenticated session |
| Status | `6e400003` | Implemented | Read + notify JSON status |
| PIN Management | `6e400004` | Implemented | Auth/change PIN/config/logs/abort/key_combo |
| WiFi Config | `6e400005` | Partial | Stub (`{"error":"not_available"}` on read) |
| Cert Fingerprint | `6e400006` | Partial | Stub (64 zeroes) |

### 1.5 Authentication and Access Control

| Capability | Status | Notes |
|---|---|---|
| 6-digit PIN validation (disallow weak patterns) | Implemented | Same constraints in firmware and webapp validator |
| Session auth required for sensitive BLE actions | Implemented | `auth` / `verify` action gates typing/config/logs |
| Rate limit and exponential backoff | Implemented | Retry delay exposed in status payload |
| Lockout after 10 failures | Implemented | Persists in NVS until reset |
| BLE link-layer security (SC/bonding/MITM) | Partial | Code path exists but current normal-mode config disables it (`sm_sc = 0`, `sm_bonding = 0`, `sm_mitm = 0`) |

### 1.6 LED and Physical Reset

| Capability | Status | Notes |
|---|---|---|
| Provisioning indicator (orange slow blink) | Implemented | `LED_STATE_PROVISIONING` |
| BLE connected indicator (blue solid) | Implemented | Set on connect |
| BOOT hold warning/confirm LEDs | Implemented | Yellow fast blink then red confirm |
| Typing LED pattern | Partial | Runtime typing path uses key-timed orange blink; `LED_STATE_TYPING` (red 500ms flash) exists but is currently unused by typing engine flow |
| WiFi/WSS/OTA LED states | Planned (Phase 3) | Enum states exist but flows are not active |

### 1.7 Reset, Serial, and Audit

| Capability | Status | Notes |
|---|---|---|
| BOOT button factory reset (10s hold) | Implemented | Wipes credentials/auth/config |
| Serial command console (115200) | Implemented | `status`, `heap`, `factory_reset`, `full_reset`, `reboot`, `help` |
| Full reset command | Implemented | Erases all known NVS namespaces including `certs` |
| Audit ring buffer + NVS persistence | Implemented | 4KB buffer, loads on boot, persists on shutdown |
| Audit retrieval via BLE action | Partial | Firmware sends log payload via status notify; web UI path is basic and limited |

## 2. Webapp Capabilities

### 2.1 App Shell and Routing

| Route | Component | Status |
|---|---|---|
| `/` | `ConnectionScreen` | Implemented |
| `/provision` | `ProvisioningScreen` | Implemented |
| `/connect` | `BleConnect` | Implemented |
| `/send` | `TextSender` | Implemented |
| `/pin` | `PinSetup` | Implemented |
| `/settings` | `Settings` | Implemented |
| `/logs` | `AuditLog` | Implemented (basic) |
| `/flash` | `FirmwareFlash` | Implemented |

### 2.2 Provisioning UX

| Capability | Status | Notes |
|---|---|---|
| Scan/connect provisioning device | Implemented | Uses name prefix + provisioning service UUID |
| PIN setup + validation + complete | Implemented | End-to-end with firmware provisioning RPC |
| WiFi fields in setup screen | Planned (Phase 3) | Firmware command exists, UI not exposed |

### 2.3 Normal BLE UX

| Capability | Status | Notes |
|---|---|---|
| Connect to normal BLE service | Implemented | Via Web Bluetooth |
| Unlock session with PIN (`auth`) | Implemented | Handles retry delay and lockout states |
| Text send + clipboard send | Implemented | Uses Text Input characteristic |
| Abort current typing | Implemented | Uses PIN action `abort` |
| Status bar (typing/auth/keyboard mount) | Implemented | Poll + notify update path |
| Settings update (typing delay/brightness) | Implemented | Uses `set_config` action |
| PIN change screen | Implemented | Uses `set` action |

### 2.4 Keyboard Utilities and Advanced Input

| Capability | Status | Notes |
|---|---|---|
| Virtual keyboard (simple/full) | Implemented | Powered by `simple-keyboard` |
| Shortcut buttons (Ctrl/Alt/Fn/nav) | Implemented | Uses generic `key_combo` action |
| SysRq panel in sender (toggle-gated) | Partial | Implemented as key combos; no firmware-native SysRq action or cooldown/confirm workflow |

### 2.5 Firmware Flashing (Web Serial)

| Capability | Status | Notes |
|---|---|---|
| Connect over Web Serial (`esptool-js`) | Implemented | Browser must support Web Serial |
| Flash from local files | Implemented | Bootloader + partition table + app slots |
| Flash from hosted release manifest | Implemented | Reads `webapp/public/firmware/releases.json` |
| Post-flash reset | Implemented | Hard reset via esptool action |

### 2.6 PWA and Browser Support

| Capability | Status | Notes |
|---|---|---|
| Vite + Preact + TypeScript | Implemented | Build scripts and app structure in place |
| PWA plugin and manifest | Implemented | `vite-plugin-pwa` configured |
| Chromium browser requirement | Implemented | Required for Web Bluetooth + Web Serial |
| Firefox/Safari support | Planned (Phase 3) | Not currently possible for required APIs |

## 3. Protocol and UUID Reference (Current)

### 3.1 Provisioning Service

- Service: `00467768-6228-2272-4663-277478268000`
- Status: `...8001`
- Error: `...8002`
- RPC Command: `...8003`
- RPC Result: `...8004`

Commands:
- `{"command":"set_pin","pin":"123456"}`
- `{"command":"set_wifi","ssid":"MyNet","password":"pass"}`
- `{"command":"complete"}`

### 3.2 Normal Service

- Service: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- Text Input: `6e400002`
- Status: `6e400003`
- PIN Management: `6e400004`
- WiFi Config (stub): `6e400005`
- Cert Fingerprint (stub): `6e400006`

PIN Management actions:
- `auth`, `verify`, `logout`
- `set` (change PIN)
- `set_config` (`typing_delay`, `led_brightness`)
- `get_logs`
- `abort`
- `key_combo`

Status payload (actual fields):
- `connected`, `typing`, `queue`, `authenticated`, `keyboard_connected`, `retry_delay_ms`, `locked_out`, optional `auth_error`

## 4. Known Gaps and Partial Items

1. WiFi and certificate characteristics are placeholders in normal BLE service.
2. BLE security is currently app-layer PIN auth; link-layer SC/bonding/MITM enforcement is disabled in normal mode.
3. Typing LED behavior differs from original red-flash spec (`LED_STATE_TYPING` exists, but runtime currently uses key-timed orange blink).
4. Web provisioning flow does not expose WiFi credential input even though firmware accepts `set_wifi`.
5. Audit log viewer is basic; firmware emits logs through notify path but UI retrieval/parsing is minimal.
6. No WSS/network transport path is wired in firmware or webapp.
7. OTA over network is not implemented.

## 5. Planned Features (Phase 3)

- WiFi manager and real BLE WiFi operations (`scan/connect/disconnect/list/forget`)
- Certificate generation/storage and real fingerprint characteristic
- HTTP cert download endpoint
- HTTPS/WSS server with PIN auth header
- OTA update transport and signature-verification flow integration
- Dedicated network connection UI and certificate setup UI
- Robust SysRq safety UX (confirmations/cooldown) plus dedicated firmware event handling
- Connectivity policy manager for BLE/WSS switching

## 6. CI/CD and Tooling

### Implemented Workflows

- `build-firmware.yml`
  - Trigger: tag push `v*` or manual dispatch
  - Build in `espressif/idf:v5.3`
  - Sign with `OTA_SIGNING_KEY`
  - Publish artifacts: `firmware-esp32s3.bin`, `firmware-esp32s3.sig`, `bootloader-esp32s3.bin`, `partition-table-esp32s3.bin`

- `deploy-webapp.yml`
  - Trigger: successful `Build Firmware` workflow-run (push event) or manual dispatch with release tag
  - Uses Node 24
  - Prepares hosted firmware assets via `.github/scripts/prepare-pages-firmware.mjs`
  - Deploys webapp to GitHub Pages

### Build Toolchain

- Firmware: ESP-IDF (`idf.py`) in devcontainer
- Webapp: Vite + TypeScript
- Flasher: Web Serial + `esptool-js`
