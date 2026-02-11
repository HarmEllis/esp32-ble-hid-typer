# ESP32 BLE HID Typer

## Project Overview

Monorepo with ESP32 firmware and Preact PWA. ESP32 acts as USB HID keyboard. PWA connects via BLE or WebSocket to send text for typing. Use cases: pasting passwords, configs, or text into machines where pasting is difficult.

PWA hosted on GitHub Pages (free HTTPS). ESP32 runs minimal HTTPS server for WSS and HTTP server for certificate download. Supports OTA firmware updates.

**Supported: ESP32-S3 only**. BLE mandatory.

All code, comments, UI, docs must be in English.

## Security Architecture

Defense-in-depth for embedded systems prioritizing functionality and safety:

### Core Features

1. **OTA Signing** - ECDSA P-256 signed firmware, public key embedded (not eFuse), app-level verification, no Secure Boot, USB flashing always allowed
2. **Certificate Trust** - Self-signed ECDSA cert, SHA256 fingerprint via BLE GATT + serial, PWA verifies before trust, prevents MITM
3. **Provisioning Mode** - First boot without PIN triggers auto provisioning, broadcasts "ESP32-HID-SETUP", LED blinks orange, user sets 6-digit PIN (validated, not 000000), optional WiFi via Improv protocol, reboots to normal mode
4. **NVS Encryption** - Partition-based (not eFuse), all sensitive data encrypted, keys in `nvs_keys` partition, hardware remains reflashable
5. **Rate Limiting** - Max 3 PIN attempts/60s, exponential backoff, lockout after 10 failures, typing max 1000 chars/min
6. **BLE Security** - LE Secure Connections, passkey pairing, reject unencrypted/legacy, auto-cleanup old bondings
7. **SysRq Protection** - Opt-in toggle, localStorage per browser/device, 10s cooldown, separate confirmation per action
8. **LED Feedback** - Visual status (off/blue/white/yellow/red), 5% brightness (configurable), GPIO48 WS2812
9. **Audit Logging** - Syslog RFC5424, 4KB RAM buffer, persisted to NVS, no sensitive data
10. **Mandatory PIN** - Must be set via provisioning, no random/default fallback, PWA validates format

### User Flow

1. **First time**: Visit PWA → flash firmware via Web Serial → power on ESP32
2. **Provisioning** (first boot): LED blinks orange → PWA detects → enter 6-digit PIN + optional WiFi → reboot
3. **Daily use (BLE)**: Connect → paste/type → types out (red LED)
4. **Certificate setup** (once): PWA reads fingerprint via BLE → download cert via HTTP → verify match → import to OS trust store → restart browser
5. **Daily use (Network)**: Enter IP + PIN → connect via WSS → paste/type
6. **Factory reset**: Hold BOOT 10s → LED yellow warning → red confirm → reset → re-provision

## Architecture

```
┌──────────────────┐                    ┌─────────────────┐     USB HID      ┌──────────────┐
│ Preact PWA       │   BLE GATT/WSS     │   ESP32-S3      │ ──────────────►  │ Target PC    │
│ (GitHub Pages)   │ ─────────────►     │                 │   Keystrokes     │              │
│                  │                    │ - USB HID       │                  └──────────────┘
│                  │                    │ - BLE Server    │   ┌──────────────┐
└──────────────────┘                    │ - WiFi AP/STA   │   │ BOOT Button  │
                                        │ - HTTPS/WSS     │◄──┤ (GPIO0)      │
                                        │ - Provisioning  │   │ Factory Reset│
                                        │ - NeoPixel LED  │   └──────────────┘
                                        └─────────────────┘
```

### Key Decisions

- **GitHub Pages** - Free HTTPS, webapp updates without firmware flashing
- **PWA-first** - Offline support, install once, use forever over BLE
- **No Secure Boot/eFuse** - Hardware fully reflashable, OTA signed at app level
- **ESP-IDF native** - idf.py build, ESP Component Registry for deps
- **DevContainer build-only** - Flashing via browser Web Serial API, no USB passthrough
- **Dual transport** - BLE (primary) + WSS (fallback), same PIN auth
- **Self-signed ECDSA** - P-256 cert, fingerprint via BLE for trust, HTTP download
- **Improv WiFi** - Standard BLE provisioning, compatible with other tools
- **OTA dual partitions** - ota_0 + ota_1 with rollback, app-level signature verification
- **TinyUSB** - Via Component Registry, GPIO19 (D-), GPIO20 (D+)
- **Factory reset** - 10s BOOT button hold with LED feedback (primary), serial console (optional)
- **SysRq keys** - Linux Magic SysRq via HID, opt-in + confirmation required
- **Audit logging** - RFC5424, RAM + encrypted NVS, retrievable via PWA
- **NeoPixel status** - WS2812 on GPIO48, 5% brightness, visual connection/typing feedback

## Supported Hardware

| Chip | USB OTG | BLE | WiFi | Status |
|------|---------|-----|------|--------|
| ESP32-S3 | GPIO19/20 | ✅ | ✅ | **Supported** |
| ESP32-P4 | Per datasheet | ✅ | ✅ | Unsupported for now |
| ESP32-S2 | — | ❌ | ✅ | **No** (no BLE) |

**Requirements**:
- USB OTG pins exposed (GPIO19/20)
- Built-in WS2812 LED (GPIO48 on most DevKits)
- BOOT button (GPIO0, standard)
- Optional: USB-UART bridge for serial console (debugging only)

**Note**: S3 USB PHY shared between OTG and Serial-JTAG. When TinyUSB init, Serial-JTAG unavailable. Separate UART bridge required for console access.

## Tech Stack

| Component | Technology |
|-----------|-----------|
| Firmware | ESP-IDF v5.3+ idf.py |
| USB HID | TinyUSB via `espressif/esp_tinyusb` |
| BLE | NimBLE (lighter than Bluedroid) |
| WiFi | ESP-IDF driver (AP+STA) |
| HTTPS/WSS | `esp_https_server` port 8443 |
| HTTP | `esp_http_server` port 80 (cert only) |
| TLS | mbedTLS ECDSA P-256 |
| OTA | `esp_https_ota` app-level signature |
| NVS | Encrypted partition-based |
| Webapp | Preact + TypeScript + Vite |
| PWA | vite-plugin-pwa (Workbox) |
| BLE API | Web Bluetooth (Chromium only) |
| Serial | esptool-js + Web Serial (Chromium) |
| Hosting | GitHub Pages |
| CI/CD | GitHub Actions |
| Dev | DevContainer ESP-IDF + Node.js |

## Repository Structure

**Phase 2 (Implemented)**: USB HID, BLE, Provisioning, LED, NVS, Auth, Serial, Typing
**Phase 3 (Planned)**: WiFi, WSS, HTTPS, Certificates, OTA

```
esp32-ble-hid-typer/
├── .devcontainer/          # DevContainer config
├── .github/workflows/      # CI/CD (deploy-webapp, build-firmware)
├── firmware/
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults*
│   ├── partitions.csv      # OTA dual partitions
│   └── main/
│       ├── main.c          # Entry, provisioning vs normal
│       ├── provisioning.*  # BLE provisioning service
│       ├── improv_wifi.*   # Improv protocol (Phase 3)
│       ├── usb_hid.*       # TinyUSB HID keyboard
│       ├── ble_server.*    # GATT server (normal)
│       ├── ble_security.*  # LE Secure, passkey, bonding
│       ├── ble_cert_service.* # Cert fingerprint (Phase 3)
│       ├── wifi_manager.*  # AP+STA, scan, connect (Phase 3)
│       ├── https_server.*  # WSS port 8443 (Phase 3)
│       ├── http_server.*   # Cert download port 80 (Phase 3)
│       ├── ws_handler.*    # WebSocket + PIN auth (Phase 3)
│       ├── tls_certs.*     # ECDSA cert gen + NVS (Phase 3)
│       ├── auth.*          # PIN storage, verification, rate limit
│       ├── ota.*           # OTA + signature verification (Phase 3)
│       ├── typing_engine.* # Text-to-HID conversion
│       ├── sysrq.*         # Magic SysRq keys
│       ├── serial_cmd.*    # Console commands (optional)
│       ├── button_reset.*  # BOOT button 10s factory reset
│       ├── audit_log.*     # Syslog RFC5424
│       ├── neopixel.*      # WS2812 LED control
│       ├── perf_monitor.*  # Heap/stack (DEBUG)
│       └── keymap_us.h     # US layout HID codes
├── webapp/
│   ├── package.json
│   ├── vite.config.ts
│   └── src/
│       ├── index.tsx
│       ├── app.tsx
│       ├── components/     # Connection, Provisioning, BLE, Network,
│       │                   # Cert, Text, Clipboard, Status, WiFi,
│       │                   # Flash, OTA, Serial, SysRq, Audit, Settings,
│       │                   # Guide, Security
│       ├── utils/          # ble, websocket, auth, fingerprint, storage
│       └── types/protocol.ts
└── docs/                   # SECURITY, OTA_SIGNING, HARDWARE, THREAT_MODEL
```

## BLE Protocol (Normal Mode)

**Service**: 6e400001-b5a3-f393-e0a9-e50e24dcca9e

**Characteristics**:
1. Text Input (Write) `6e400002` - Max 512 bytes
2. Status (Read, Notify) `6e400003` - JSON: `{"connected":true,"typing":false,"queue":0,"pin_set":true}`
3. PIN Management (Write) `6e400004` - JSON: `{"action":"set","old":"123456","new":"654321"}`
4. WiFi Config (Write, Read) `6e400005` - **(Phase 3)** JSON actions: scan, connect, disconnect, list, forget
5. Cert Fingerprint (Read) `6e400006` - **(Phase 3)** 64-char hex SHA256

**Security**: PIN pairing, LE Secure Connections, reject unencrypted/legacy, auto-cleanup bonding, rate limit 3/60s, lockout after 10

**Implementation Status**: Characteristics 1-3 fully functional. Characteristics 4-5 return stub/placeholder data until Phase 3.

## Provisioning Mode

Triggers on first boot or factory reset (no PIN in NVS).

**Indicators**: Orange LED slow blink (1s on/off), broadcasts "ESP32-HID-SETUP", no USB HID, console log

**BLE Service**: `00467768-6228-2272-4663-277478268000` (Improv WiFi compatible)

**Characteristics**:
- Status `...001`: uint8 (0=ready, 1=provisioning, 2=provisioned)
- Error `...002`: uint8 (0=none, 1=invalid_pin, 2=unable_to_connect, 3=unknown)
- RPC Command `...003`: JSON write
- RPC Result `...004`: JSON read/notify

**Commands**:
```json
{"command":"set_pin","pin":"123456"}
{"command":"set_wifi","ssid":"MyNet","password":"pass"}
{"command":"complete"}
```

**PIN Validation**: 6 digits, not 000000, not sequential (123456, 654321), not repetitive (111111)

**After complete**: Reboot to normal mode

**Improv WiFi Compatibility**: Generic Improv tools can set WiFi only, PIN must be set separately

## WebSocket Protocol **(Phase 3 - Planned)**

**Connection**: `wss://<ip>:8443/ws`, Header: `Authorization: PIN <6-digit>`

**Messages** (JSON):
```json
// Client → ESP32
{"type":"text","data":"Hello"}
{"type":"wifi_scan","data":{}}
{"type":"wifi_connect","data":{"ssid":"...","password":"..."}}
{"type":"ota_update","data":{"url":"https://..."}}

// ESP32 → Client
{"type":"status","data":{"connected":true,"typing":false,"queue":0}}
{"type":"wifi_scan_result","data":[{"ssid":"...","rssi":-45,"secure":true}]}
{"type":"typing_progress","data":{"current":50,"total":100}}
{"type":"error","data":{"message":"..."}}
```

**Security**: TLS 1.2+, cert fingerprint verified, PIN per connection, rate limit 3/60s, 5min idle timeout

## Certificate Setup Flow **(Phase 3 - Planned)**

1. ESP32 boot → generate ECDSA P-256 cert (if missing) → store encrypted NVS → calc SHA256 → print to serial
2. PWA via BLE → read fingerprint `6e400006` → display
3. PWA download → HTTP GET `http://<ip>/cert.pem` → calc SHA256 → compare
4. If match → show import instructions, else abort + suggest factory reset
5. User import → OS trust store (macOS Keychain, Windows certmgr, Linux `/usr/local/share/ca-certificates/`)
6. Restart browser → WSS works without warnings

## OTA Update

**USB Flash**: Always allowed, any firmware, no signature required (implemented)

**OTA (Phase 3 - Planned)**: Signed ECDSA P-256, dual partitions (ota_0/ota_1), rollback on crash

**Signing**:
```bash
openssl ecparam -genkey -name prime256v1 -out ota_signing_key.pem
openssl ec -in ota_signing_key.pem -pubout -out ota_signing_pubkey.pem
openssl dgst -sha256 -sign ota_signing_key.pem -out firmware.sig firmware.bin
```
Public key embedded in firmware, private key in GitHub Secrets. See `docs/OTA_SIGNING.md`.

## Typing Engine

**Conversion**: UTF-8 → HID reports via keymap_us.h

**Rate**: Configurable delay (default 10ms/keystroke, 50ms/word), max 1000 chars/min, adjustable 5-100ms

**Queue**: 8KB RAM, char-by-char, progress notifications, abort command

## LED Status

| State | Color | Pattern | Meaning |
|-------|-------|---------|---------|
| Provisioning | Orange | Slow blink 1s | Waiting for setup |
| Disconnected | Off | Solid | No connection |
| BLE | Blue | Solid | Connected idle |
| WiFi | White | Solid | Connected no WSS |
| WSS | Yellow | Solid | Connected idle |
| Typing | Red | Flash 500ms | Active |
| Reset Warning | Yellow | Flash 100ms | BOOT 2-10s |
| Reset Confirm | Red | Solid 1s | BOOT 10s+ |
| Error | Red | Flash 100ms | No PIN |
| OTA | Purple | Pulse | Updating |

Hardware: GPIO48 WS2812, 5% brightness (1-100% configurable)

**Implementation**: Use `espressif/led_strip` component via RMT peripheral

## Audit Logging

**Format**: Syslog RFC5424
```
<134>1 2024-02-08T12:00:00.000Z esp32-hid - auth_attempt - - transport=ble result=fail reason=invalid_pin
```

**Storage**: 4KB RAM ring buffer (~100 events) → persist to encrypted NVS at reboot → load at boot

**Events**: Auth attempts, OTA updates, SysRq exec, factory resets, cert regen, PIN changes, WiFi connect/disconnect (no sensitive data)

**Retrieval**: Via PWA (BLE or WSS) - display, export, clear

**Future**: Remote TLS syslog (RFC5425), auto-push if WiFi connected

## SysRq Implementation

**GATT**: `6e400007` Write `{"action":"sysrq","key":"h"}`

**WSS**: `{"type":"sysrq","data":{"key":"h"}}`

**HID Sequence**: Alt + PrintScreen + key

**Supported Keys**: h=help, b=reboot, c=crash, d=locks, e=SIGTERM, f=OOM, i=SIGKILL, k=SAK, m=memory, n=nice, o=poweroff, p=registers, q=timers, r=keyboard, s=sync, t=tasks, u=remount-ro, v=ETM, w=blocked, z=ftrace

**REISUB**: PWA button sends R-E-I-S-U-B with 2s delays for safe reboot

**Safety**: Opt-in toggle (localStorage), confirmation dialog per action, 10s cooldown, audit logged

## Factory Reset

**Primary method**: Hold BOOT button (GPIO0)

**LED Pattern**:
- 0-2s: Normal LED
- 2-10s: Yellow rapid flash (warning, release to cancel)
- 10s: Red solid 1s (confirmed)
- After: Reboot to provisioning mode (orange slow blink)

**Effect**: Wipe PIN, WiFi creds, AP settings, reboot to provisioning. Certificate + bonding kept.

**Optional serial**: `factory_reset` command (same as BOOT), `full_reset` (also regenerates cert + wipes bonding/audit)

**⚠️ Important**: After `full_reset`, new certificate generated with NEW fingerprint. User MUST repeat certificate verification flow (read new fingerprint via BLE, download cert, verify match, re-import to OS trust store) before WSS connections will work.

**Advantages BOOT over serial**: No UART bridge, no terminal software, visual feedback, cancellable, physical access required

## Serial Console (Optional)

**Not required** for normal operation. All essential functions (flash, provision, reset) via PWA + BOOT button.

**Baud**: 115200 8N1

**Commands** (currently implemented): `status`, `heap`, `factory_reset`, `full_reset`, `reboot`, `help`

**Phase 3 additions**: `cert_fingerprint`, `wifi_scan`

## Build Commands

```bash
# Firmware
cd firmware && idf.py set-target esp32s3 && idf.py build

# Webapp dev
cd webapp && npm run dev

# Webapp prod
cd webapp && npm run build

# Sign firmware (manual)
openssl dgst -sha256 -sign ota_signing_key.pem -out build/firmware.sig build/esp32-ble-hid-typer.bin
```

## GitHub Actions CI/CD

**deploy-webapp.yml**: Triggers on successful Build Firmware completion or manual dispatch → build with Node 24 → prepare firmware assets from releases → deploy to GitHub Pages

**build-firmware.yml**: Triggers on tags `v*` or manual dispatch → build in ESP-IDF v5.3 container → sign with ECDSA key from GitHub Secrets → rename to chip-specific names → upload artifacts (firmware-esp32s3.bin, firmware-esp32s3.sig, bootloader-esp32s3.bin, partition-table-esp32s3.bin)

Private key stored as `OTA_SIGNING_KEY` secret. Public key embedded in firmware source.

## Testing Strategy

- USB HID: `lsusb`, Device Manager, text editor output
- LED: Verify colors/patterns, brightness 5%
- Provisioning: Flash → orange blink → "ESP32-HID-SETUP" → PIN validation → WiFi → reboot
- BLE: nRF Connect after provisioning, pairing requires PIN
- Cert: Fingerprint via BLE matches HTTP download, PWA rejects mismatch
- WSS: Import cert → restart browser → connect
- OTA: Signature verification, rollback on fail
- Audit: Events logged, Syslog format, persist across reboot
- Rate limit: Lockout after 10 fails, exponential backoff
- BOOT reset: 2s yellow → 10s red → reboot provisioning → re-provision works
- SysRq: Opt-in, confirmation, cooldown, Linux SysRq+h
- Settings: Typing speed, LED brightness
- Performance: Heap monitoring (DEBUG), no leaks
- NVS: PIN encrypted
- E2E: Provision → normal → type → factory reset → re-provision

## Implementation Order

1. DevContainer setup
2-7. Firmware: USB HID, LED, NVS, provisioning detection, BLE service, commands, LED blink
8-12. Webapp: PWA shell, flasher, provisioning screen, UI, BLE commands
13-19. Firmware+Webapp: Normal mode, BLE GATT, PIN, typing, LED feedback
20-26. Firmware+Webapp: Cert gen, fingerprint, WiFi, HTTPS/WSS/HTTP, cert download
27-32. Firmware+Webapp: Audit log, viewer, settings, BOOT reset, serial, monitor
33-36. Firmware+Webapp: SysRq, OTA, security info
37-42. Firmware+Webapp: Perf monitor, Improv compliance, guide/polish, CI/CD, docs, E2E testing

## Security Checklist

- [ ] Provisioning auto-triggers (no PIN)
- [ ] BLE broadcasts "ESP32-HID-SETUP"
- [ ] Orange LED slow blink
- [ ] PIN validation (6 digits, not 000000/sequential/repetitive)
- [ ] Provisioning commands work
- [ ] Normal mode only after PIN set
- [ ] Rate limiting (3/60s, lockout 10)
- [ ] BLE LE Secure Connections
- [ ] Cert fingerprint via BLE
- [ ] PWA verifies fingerprint
- [ ] ECDSA P-256
- [ ] NVS partition encryption
- [ ] OTA signature verification
- [ ] LED typing feedback (red flash)
- [ ] LED reset warning (yellow 2-10s)
- [ ] SysRq opt-in + confirmation
- [ ] Audit logging (Syslog, encrypted NVS)
- [ ] BOOT reset works (10s LED feedback)
- [ ] Reset returns to provisioning
- [ ] Serial optional
- [ ] No sensitive data in logs
- [ ] Threat model documented
- [ ] Security review done
- [ ] Improv WiFi compatible

## Known Limitations

1. **Browser**: Web Bluetooth/Serial = Chromium only (Chrome, Edge, Opera). No Firefox/Safari.
2. **Flash protection**: No Secure Boot, no eFuse burning (intentional for reflashability). OTA signed at app level.
3. **Certificate**: Manual import to OS trust store required (self-signed).
4. **BLE range**: 10-30m typical. Use WiFi/WSS for longer.
5. **Typing speed**: Limited by HID poll rate. Not for large documents.
6. **Flash wear**: NVS wear leveling helps, but finite write cycles. Audit log minimizes writes.
7. **Physical access**: NVS keys readable from `nvs_keys` partition without eFuse. Firmware flashable. Trade-off for open-source reflashability.
8. **Cert rotation**: No auto-rotation. `full_reset` via serial regenerates (wipes all).
9. **Provisioning security**: BLE open during provisioning (no pairing), secure after reboot.
10. **Serial optional**: `full_reset`, `heap`, `wifi_scan` need serial. All essential functions via PWA/BOOT.

## Connectivity Behaviour

**Design**: Device is operator tool, not permanent keyboard. Minimize friction, reduce attack surface, user-configurable.

**Default**: Single active transport. BLE enabled → WSS connects → BLE disabled → WSS disconnects → BLE re-enabled.

**Settings** (encrypted NVS):
- Disable BLE when WiFi connected (default ON)
- Permanently disable BLE (default OFF)
- Enable BLE when WiFi not connected (default ON)

**Implementation**:
```c
void update_ble_state() {
    if (config.ble_force_disabled) { ble_stop(); return; }
    if (wifi_ws_connected && config.ble_auto_disable_on_wifi) { ble_stop(); return; }
    if (!wifi_connected && config.ble_enable_when_no_wifi) { ble_start(); return; }
}
```

**Rationale**: Prevents simultaneous BLE+WiFi command channels, reduces attack surface, UX friction-free, user risk tolerance.

**Security Note**: All typing commands on both BLE and WSS transports require PIN authentication and are encrypted (BLE via LE Secure Connections, WSS via TLS 1.2+). Disabling one transport does not weaken security of the active transport.

## Future Enhancements

- ESP32-P4 support
- Remote TLS syslog
- Multiple layouts (DE, NL, FR)
- Text snippets (encrypted NVS)
- TOTP generator
- Password generator
- Cert auto-rotation
- Captive portal setup
- E-ink display for PIN
- HSM (ESP32-C6/H2 secure element)

## License

MIT - see LICENSE file

## Contributing

PRs welcome. For major changes, open issue first.

## Support

- GitHub Issues: Bugs, features
- Discussions: Q&A, community
- Security: Email security@example.com (private)

---

**Read FEATURES.md and PLAN.md → Phase 3 NOT implemented yet**
