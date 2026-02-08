# ESP32 BLE HID Typer — Feature Overview

Structured overview of all features, grouped by domain.

---

## 1. USB HID Keyboard

| Feature | Description |
|---|---|
| USB HID device | ESP32-S3 presents itself as a standard USB keyboard via TinyUSB |
| Typing engine | Converts UTF-8 text to HID keycodes (US layout) |
| Rate limiting | Configurable 5-100ms delay per keystroke, max 1000 chars/min |
| Text queue | Max 8KB RAM buffer, FIFO processing |
| Abort | Clear queue and stop typing immediately |
| Progress | Progress notifications (current/total) via BLE and WSS |
| USB OTG pins | GPIO19 (D-), GPIO20 (D+) — native USB |

---

## 2. Provisioning (Initial Setup)

| Feature | Description |
|---|---|
| Automatic detection | No PIN in NVS → provisioning mode at boot |
| BLE broadcast | Device advertises as "ESP32-HID-SETUP" |
| LED indicator | Orange slow blink (1s on/off) |
| PIN setup | Mandatory 6 digits, not 000000, not sequential, not repetitive |
| WiFi setup | Optional SSID + password via BLE |
| Improv WiFi | Compatible with Improv WiFi protocol for generic tools |
| Complete | After provisioning → automatic reboot into normal mode |
| USB HID disabled | No keyboard functionality during provisioning |

---

## 3. Bluetooth Low Energy (BLE)

### 3.1 GATT Services — Normal Mode

| Characteristic | UUID (short) | Permissions | Purpose |
|---|---|---|---|
| Text Input | `6e400002` | Write | Send text to type |
| Status | `6e400003` | Read, Notify | Connection status and typing progress |
| PIN Management | `6e400004` | Write | Change and verify PIN |
| WiFi Config | `6e400005` | Write, Read | Scan, connect, manage WiFi |
| Certificate Fingerprint | `6e400006` | Read | SHA256 fingerprint of TLS certificate |
| SysRq | `6e400007` | Write | Send Linux SysRq magic keys |

### 3.2 GATT Services — Provisioning Mode

| Characteristic | UUID (short) | Permissions | Purpose |
|---|---|---|---|
| Status | `...8001` | Read, Notify | Provisioning status (ready/provisioning/provisioned) |
| Error | `...8002` | Read, Notify | Error codes (invalid_pin, unable_to_connect, unknown) |
| RPC Command | `...8003` | Write | Provisioning commands (set_pin, set_wifi, complete) |
| RPC Result | `...8004` | Read, Notify | Command responses (JSON) |

### 3.3 Security

| Feature | Description |
|---|---|
| LE Secure Connections | Mandatory, no legacy pairing |
| Passkey pairing | PIN as passkey for OS pairing prompt |
| Encrypted connections | Unencrypted connections are rejected |
| Bonding | Only most recent bonding kept |
| Forced PIN change | Mandatory PIN change on first connection after provisioning |

---

## 4. WiFi & WebSocket

### 4.1 WiFi Manager

| Feature | Description |
|---|---|
| AP+STA coexistence | Simultaneous access point and station mode |
| Network scan | Retrieve available networks with RSSI and encryption type |
| Connect | Connect to network via SSID + password |
| Saved networks | Management of stored WiFi credentials in encrypted NVS |
| Forget | Remove individual network from saved list |
| Configuration via BLE | WiFi management via BLE GATT characteristic |

### 4.2 HTTPS / WSS Server

| Feature | Description |
|---|---|
| HTTPS server | Port 8443, ECDSA P-256 self-signed certificate |
| WSS endpoint | `wss://<ip>:8443/ws` |
| PIN authentication | `Authorization: PIN <code>` header per connection |
| Idle timeout | 5 minutes of inactivity → disconnect |
| TLS 1.2+ | Minimum TLS version enforced |

### 4.3 HTTP Server (Certificate Download)

| Feature | Description |
|---|---|
| HTTP server | Port 80, only for certificate download |
| `/cert.pem` | PEM format certificate |
| `/cert.crt` | DER format certificate |
| No other endpoints | Minimal attack surface |

### 4.4 WebSocket Protocol

**Client → ESP32:**

| Type | Purpose |
|---|---|
| `text` | Send text to type |
| `wifi_scan` | Scan WiFi networks |
| `wifi_connect` | Connect to WiFi network |
| `ota_update` | Start OTA firmware update |
| `sysrq` | Send SysRq magic key |
| `abort` | Stop current typing action |
| `get_logs` | Retrieve audit log |
| `ping` | Heartbeat |

**ESP32 → Client:**

| Type | Purpose |
|---|---|
| `status` | Connection status and queue info |
| `wifi_scan_result` | List of discovered networks |
| `typing_progress` | Typing progress (current/total) |
| `error` | Error message |
| `logs` | Audit log data |
| `pong` | Heartbeat response |

### 4.5 Connectivity Manager

| Setting | Default | Description |
|---|---|---|
| Disable BLE when WiFi connected | On | Prevents simultaneous BLE + WiFi command channels |
| Permanently disable BLE | Off | Disable BLE entirely |
| Enable BLE when WiFi not connected | On | Automatically enable BLE as fallback |

---

## 5. Security

### 5.1 Certificate Trust

| Feature | Description |
|---|---|
| ECDSA P-256 certificate | Self-signed, generated at first boot |
| SHA256 fingerprint | Available via BLE and serial console |
| Fingerprint verification | PWA compares BLE fingerprint with downloaded certificate |
| Certificate download | Via HTTP (unencrypted), secured by fingerprint verification |
| OS import | Manual import into trust store (macOS, Windows, Linux) |

### 5.2 PIN & Authentication

| Feature | Description |
|---|---|
| 6-digit PIN | Mandatory, set via provisioning flow |
| Rate limiting | Max 3 attempts per 60 seconds |
| Exponential backoff | Increasing delay after failed attempts |
| Device lockout | After 10 failed attempts, requires physical reset |
| No default PIN | PIN must be consciously chosen by user |

### 5.3 OTA Firmware Signing (Application-level)

| Feature | Description |
|---|---|
| ECDSA P-256 signature | Application-level OTA signature verification |
| Embedded public key | Public key compiled into firmware binary (not in eFuse) |
| OTA verification | Firmware verifies signature before accepting OTA update |
| USB flash unrestricted | USB flashing always allowed with any firmware |
| No Secure Boot | No eFuse burning — hardware remains fully reflashable |
| Open-source friendly | Users can always repurpose their hardware with other software |

### 5.4 NVS Encryption (Partition-based)

| Feature | Description |
|---|---|
| Encrypted NVS | All sensitive data encrypted in flash |
| Partition-based keys | Encryption keys stored in `nvs_keys` partition (not in eFuse) |
| Software-level protection | Protects against casual flash readout |
| No eFuse burning | Hardware remains fully reflashable |
| Stored data | PIN, WiFi credentials, certificates, audit log, configuration |
| Trade-off | Without flash encryption (eFuse), keys are readable with physical access |

---

## 6. NeoPixel LED Feedback

| State | Color | Pattern |
|---|---|---|
| Provisioning mode | Orange | Slow blink (1s on/off) |
| Not connected | Off | — |
| BLE connected | Blue | Solid |
| WiFi connected | White | Solid |
| WSS connected | Yellow | Solid |
| Typing active | Red | Flashing (500ms on/off) |
| Factory reset warning | Yellow | Rapid flash (100ms on/off) |
| Factory reset confirmed | Red | Solid (1s) |
| Error | Red | Rapid blink (100ms on/off) |
| OTA update | Purple | Pulsing |

- Default brightness: 5%, configurable 1-100% via PWA
- Hardware: WS2812 NeoPixel on GPIO48 (ESP32-S3 DevKit)

---

## 7. Factory Reset

### 7.1 Via BOOT Button (primary)

| Step | Timing | LED |
|---|---|---|
| Normal | 0-2 seconds | Current status |
| Warning | 2-10 seconds | Yellow rapid flash |
| Confirmed | 10+ seconds | Solid red (1s) |
| Cancel | Release before 10s | Return to normal |

Wipes: PIN, WiFi credentials, AP settings. Preserves: certificate, bonding.

### 7.2 Via Serial Console (optional)

| Command | Effect |
|---|---|
| `factory_reset` | Same as BOOT button (wipes PIN, WiFi) |
| `full_reset` | + New certificate + wipe bonding + wipe audit log |

---

## 8. OTA Firmware Updates

| Feature | Description |
|---|---|
| Dual partitions | `ota_0` + `ota_1` with rollback support |
| App-level signature verification | Only correctly signed firmware accepted via OTA (ECDSA P-256) |
| Embedded public key | Verification key compiled into firmware, not in eFuse |
| Download source | GitHub Releases (firmware + detached signature) |
| Rollback | Automatic on crash after update |
| USB flash unrestricted | USB flashing always allowed with any firmware (no Secure Boot) |
| LED indicator | Purple pulsing during update |

---

## 9. SysRq Magic Keys (Linux)

| Feature | Description |
|---|---|
| HID sequence | Alt + PrintScreen + key |
| Supported keys | h, b, c, d, e, f, i, k, m, n, o, p, q, r, s, t, u, v, w, z |
| REISUB | Automated safe reboot sequence with 2s delays |
| Opt-in | Hidden behind toggle in settings (localStorage) |
| Confirmation | Per action: dialog + checkbox + 10s cooldown |
| Audit logging | Every use is logged |

---

## 10. Audit Logging

| Feature | Description |
|---|---|
| Format | Syslog RFC5424 |
| Storage | 4KB ring buffer in RAM |
| Persistence | To encrypted NVS at reboot |
| Events | Auth attempts, OTA, SysRq, factory resets, PIN changes, WiFi |
| Retrieval | Via PWA (BLE or WSS) |
| Sensitive data | Never logged (no PINs, passwords, typed text) |
| Future | Remote syslog server (TLS) |

---

## 11. Serial Console (Optional)

| Command | Description |
|---|---|
| `status` | Connection status, WiFi, IP, uptime, heap |
| `cert_fingerprint` | Display SHA256 fingerprint |
| `factory_reset` | Factory reset (same as BOOT button) |
| `full_reset` | Full reset including certificate |
| `reboot` | Restart device |
| `heap` | Detailed heap usage |
| `wifi_scan` | Scan WiFi networks |
| `help` | Show available commands |

Configuration: 115200 baud, 8N1. Requires separate USB-UART bridge.

---

## 12. PWA (Preact Web App)

### 12.1 Screens

| Screen | Purpose |
|---|---|
| ConnectionScreen | Mode detection, BLE or Network choice |
| ProvisioningScreen | Initial setup: PIN + WiFi via BLE |
| BleConnect | BLE connection in normal mode |
| NetworkConnect | IP + PIN input for WSS |
| CertificateSetup | Certificate download + fingerprint verification |
| PinSetup | Mandatory PIN change |
| TextSender | Enter and send text |
| ClipboardPaste | Paste & send from clipboard |
| StatusBar | Connection status and typing progress |
| WifiConfig | WiFi scan, connect, manage |
| FirmwareFlash | Flash firmware via Web Serial |
| OtaUpdate | OTA update via WiFi |
| SerialMonitor | Serial console (optional) |
| SysRqPanel | SysRq menu with safety controls |
| AuditLog | Audit log viewer |
| Settings | Settings (speed, LED, PIN, SysRq) |
| Guide | Setup instructions and explanations |
| SecurityWarning | Security information and warnings |

### 12.2 Technology

| Component | Technology |
|---|---|
| Framework | Preact + TypeScript |
| Bundler | Vite |
| PWA | vite-plugin-pwa (Workbox) |
| BLE | Web Bluetooth API |
| Serial | Web Serial API + esptool-js |
| Hosting | GitHub Pages (free HTTPS) |
| Offline | Service worker for full offline support |

### 12.3 Browser Compatibility

- Chromium required: Chrome, Edge, Opera (for Web Bluetooth and Web Serial)
- Firefox and Safari: not supported

---

## 13. CI/CD (GitHub Actions)

| Workflow | Trigger | Action |
|---|---|---|
| `deploy-webapp.yml` | Push to `main` (path: `webapp/**`) | Build PWA → deploy to GitHub Pages |
| `build-firmware.yml` | Push of tag `v*` | Build firmware → sign for OTA → GitHub Release |

---

## 14. Development Environment

| Component | Technology |
|---|---|
| Container | DevContainer (VS Code) |
| Firmware toolchain | ESP-IDF v5.3+ in Docker image |
| Webapp toolchain | Node.js 20 LTS |
| Flashing | Via browser (Web Serial), no USB passthrough |
| OS support | Linux, macOS, Windows (via DevContainer) |

---

## 15. Supported Hardware

| Chip | BLE | WiFi | USB OTG | Status |
|---|---|---|---|---|
| ESP32-S3 | ✅ | ✅ | GPIO19/20 | **Supported** |
| ESP32-P4 | ✅ | ✅ | Per datasheet | Planned |
| ESP32-S2 | ❌ | ✅ | — | Not supported (no BLE) |

**Board requirements:** USB OTG pins exposed, WS2812 NeoPixel (GPIO48), BOOT button (GPIO0).