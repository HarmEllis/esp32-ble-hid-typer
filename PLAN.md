# ESP32 BLE HID Typer — Implementation Plan and Status Tracker

This plan is a living tracker for what is already implemented versus what remains.

## Snapshot

- Repository state reviewed against current code in `firmware/main/*` and `webapp/src/*`.
- Focus remains Phase 2 BLE-first delivery.

## Phase Summary

| Phase | Scope | Status |
|---|---|---|
| Phase 1 | Dev environment, build tooling, CI/CD baseline | Done |
| Phase 2 | BLE provisioning + BLE typing product | Mostly done (some partial items) |
| Phase 3 | WiFi/WSS/certificate/OTA transport features | Not started |

## Phase 1 — Tooling and CI/CD

### Goals

- Monorepo buildable for firmware and webapp.
- Repeatable CI workflows for firmware release and pages deployment.

### Status

| Item | Status | Notes |
|---|---|---|
| Devcontainer with ESP-IDF + Node toolchain | Done | `.devcontainer/devcontainer.json` and Dockerfile flow are active |
| Firmware builds with `idf.py build` | Done | Current firmware binary is generated in `firmware/build/` |
| Webapp builds with Vite/TS | Done | `npm run build` configured |
| Firmware release workflow | Done | `.github/workflows/build-firmware.yml` |
| Pages deploy workflow | Done | `.github/workflows/deploy-webapp.yml` |
| Hosted firmware asset preparation for web flasher | Done | `.github/scripts/prepare-pages-firmware.mjs` |

### Validation Checklist

- [x] `idf.py set-target esp32s3 && idf.py build`
- [x] `cd webapp && npm run build`
- [x] Firmware artifacts renamed to chip-specific names in CI
- [x] Pages deploy consumes hosted firmware assets manifest

## Phase 2 — BLE Product (Current Release Scope)

### 2.1 Firmware Runtime and Core Services

| Item | Status | Notes |
|---|---|---|
| Mode selection by stored PIN (provisioning vs normal) | Done | `main.c` boot flow |
| USB HID keyboard (TinyUSB) | Done | `usb_hid.c` |
| Typing engine queue + callbacks + abort | Done | `typing_engine.c` |
| LED control and reset indicators | Done | `neopixel.c`, `button_reset.c` |
| `LED_STATE_TYPING` red-flash path wired into active typing flow | Partial | State exists in LED module, but typing engine currently uses key-timed indicator path |
| Encrypted NVS storage path | Done | Uses `nvs_keys` partition when available |
| Fallback when NVS keys partition missing | Done | Falls back to unencrypted NVS with warning |

### 2.2 Provisioning Mode (BLE)

| Item | Status | Notes |
|---|---|---|
| Provisioning BLE service + characteristics | Done | `provisioning.c` |
| PIN setup command + validation | Done | `set_pin` |
| Optional WiFi credential storage command | Done | `set_wifi` stores credentials only |
| Provisioning complete + reboot | Done | `complete` command |
| Provisioning UI in webapp | Partial | UI covers PIN setup + complete, no WiFi inputs yet |

### 2.3 Normal BLE Service

| Item | Status | Notes |
|---|---|---|
| Text Input (`6e400002`) | Done | Auth-gated writes |
| Status (`6e400003`) | Done | JSON status + notify |
| PIN Management (`6e400004`) | Done | `auth/verify/logout/set/set_config/get_logs/abort/key_combo` |
| WiFi Config (`6e400005`) | Partial | Stub only |
| Cert Fingerprint (`6e400006`) | Partial | Placeholder only |

### 2.4 Authentication and Security

| Item | Status | Notes |
|---|---|---|
| PIN format rules and storage | Done | `auth.c`, `nvs_storage.c` |
| Rate limiting + exponential backoff | Done | `auth_get_retry_delay_ms()` |
| Lockout after repeated failures | Done | Persisted in NVS |
| App-layer auth gate on actions | Done | Enforced in BLE handlers |
| BLE link-layer security enforcement (SC/bonding/MITM) in normal mode | Partial | Current code disables this path (`sm_sc = 0`, `sm_bonding = 0`, `sm_mitm = 0`) and relies on app-layer PIN auth |

### 2.5 Operational Features

| Item | Status | Notes |
|---|---|---|
| BOOT-button factory reset | Done | 10s hold with warning/confirm LED |
| Serial console commands | Done | `status`, `heap`, `factory_reset`, `full_reset`, `reboot`, `help` |
| Audit logging ring buffer + persistence | Done | `audit_log.c` |
| Audit log retrieval in PWA | Partial | Basic fetch flow; robust notify parsing/export UX still thin |

### 2.6 Webapp BLE UX

| Item | Status | Notes |
|---|---|---|
| Connect and unlock with PIN | Done | `BleConnect.tsx` |
| Send text + clipboard + abort | Done | `TextSender.tsx`, `ClipboardPaste.tsx` |
| Status bar and polling/notify updates | Done | `StatusBar.tsx` |
| Settings sync (typing delay + brightness) | Done | `Settings.tsx` with `set_config` |
| PIN change screen | Done | `PinSetup.tsx` |
| Virtual keyboard and shortcut helpers | Done | `VirtualKeyboard.tsx` + combo actions |
| SysRq safety workflow (cooldown/confirm) | Partial | Toggle exists; full guarded workflow not implemented |

### Phase 2 Exit Criteria

- [x] BLE provisioning works end-to-end
- [x] BLE unlock and typing works end-to-end
- [x] BOOT reset works with LED feedback
- [x] Settings and PIN update paths work over BLE
- [~] Security target: decide/implement final link-layer BLE security policy (currently app-layer auth mode with `sm_sc/sm_bonding/sm_mitm` disabled)
- [~] Audit log UX maturity (currently basic)
- [~] Provisioning WiFi UX (firmware accepts command, UI missing)

## Phase 3 — Planned Work (Not Implemented)

### 3.1 Networking and Certificate Stack

- [ ] Add WiFi manager module and real operations behind BLE/WSS APIs.
- [ ] Implement certificate generation/storage and real SHA-256 fingerprint output.
- [ ] Add HTTP certificate download endpoint(s).
- [ ] Add HTTPS server with WSS endpoint and network auth flow.

### 3.2 Transport and OTA

- [ ] Add WebSocket protocol handlers for text/status/network operations.
- [ ] Implement OTA network update path integrated with signature verification.
- [ ] Implement BLE/WSS transport policy manager and configuration.

### 3.3 Webapp Phase 3 Screens

- [ ] Network connect screen.
- [ ] Certificate setup/verification screen.
- [ ] WiFi management screen.
- [ ] OTA update screen.
- [ ] Optional serial monitor and security guide screens.

### 3.4 Security Hardening Follow-up

- [ ] Decide and implement final BLE link-layer security policy for normal mode.
- [ ] Add explicit SysRq safety controls (confirmation + cooldown) if retained as product feature.
- [ ] Expand threat-model docs once network stack exists.

## Immediate Next Actions (Recommended)

1. Close Phase 2 partials:
- Add provisioning WiFi inputs to `ProvisioningScreen.tsx` (reuse existing firmware command).
- Improve audit-log retrieval path in webapp to consume notify payload robustly.
- Resolve BLE security-mode decision and document it explicitly.

2. Start smallest useful Phase 3 slice:
- Implement real cert fingerprint characteristic first.
- Add cert generation/storage and serial visibility.
- Then add HTTP cert download endpoint.

This sequencing keeps BLE product stable while opening certificate/WSS work in controlled increments.
