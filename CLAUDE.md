# ESP32 BLE HID Typer

## Project Overview

A monorepo containing ESP32 firmware and a Preact web application. The ESP32 acts as a USB HID keyboard. A Preact PWA connects to it via Bluetooth Low Energy or WebSocket and sends text that the ESP32 types out over USB. Use cases include pasting passwords, configs, or any text into machines where you can't easily paste.

The PWA is hosted on GitHub Pages (free HTTPS). The ESP32 runs a minimal HTTPS server for WSS (WebSocket Secure) and a plain HTTP server for certificate download — it does not serve the full webapp. Firmware can be updated over-the-air (OTA) from the PWA.

**Supported chips: ESP32-S3 only** (ESP32-P4 support planned for future). BLE is mandatory — no WiFi-only mode.

All code, comments, UI text, documentation, and this file must be written in English.

## Security Architecture

This project implements defense-in-depth security for embedded systems where functionality and safety are critical:

### Core Security Features

1. **OTA Firmware Signing (Application-level)**
   - ECDSA P-256 signed firmware for OTA updates
   - Public key embedded in firmware binary (not in eFuse)
   - Application-level signature verification during OTA downloads
   - No Secure Boot — hardware remains fully reflashable with any software
   - First flash and USB flashing always allowed (unsigned)

2. **Certificate Trust via BLE Fingerprint Exchange**
   - Self-signed ECDSA certificate generated at first boot
   - SHA256 fingerprint exposed via BLE GATT characteristic
   - Fingerprint also printed to serial console at boot
   - User downloads certificate via HTTP, PWA verifies fingerprint match
   - Prevents MITM attacks during certificate setup

3. **Provisioning Mode for Initial Setup**
   - First boot without PIN → automatic provisioning mode
   - BLE service broadcasts "ESP32-HID-SETUP"
   - Built-in LED blinks blue (provisioning indicator)
   - PWA detects provisioning mode and shows setup UI
   - User sets 6-digit PIN (mandatory, validated, not 000000)
   - Optional: WiFi credentials via Improv WiFi protocol
   - After provisioning → reboot into normal mode
   - Factory reset → back to provisioning mode

4. **NVS Encryption (Partition-based)**
   - All sensitive data encrypted in flash (PIN, WiFi credentials, certificates)
   - Encryption keys stored in `nvs_keys` partition (not in eFuse)
   - Provides software-level protection against casual flash readout
   - Note: Without flash encryption (eFuse), keys are readable from flash with physical access — this is a conscious trade-off to keep hardware reflashable

5. **Rate Limiting & Authentication**
   - Max 3 PIN attempts per 60 seconds
   - Exponential backoff after failures
   - Device lockout after 10 failed attempts (requires physical reset)
   - Typing rate limited to 1000 chars/minute

6. **BLE LE Secure Connections**
   - Encrypted BLE connections with passkey pairing
   - Reject unencrypted connections and legacy pairing
   - Only most recent bonding kept (auto-cleanup)

7. **SysRq Protection**
   - Kernel-level SysRq commands behind explicit opt-in toggle
   - Stored in localStorage (opt-in per browser/device)
   - Warning dialog with mandatory 10-second cooldown
   - Separate confirmation per SysRq action

8. **Visual Feedback (Built-in NeoPixel LED)**
   - Off: Not connected
   - Blue: BLE connected
   - White: WiFi connected
   - Yellow: WebSocket connected
   - Red flashing: Typing in progress
   - Brightness: 5% (configurable via PWA)
   - Hardware: ESP32-S3 built-in WS2812 NeoPixel (GPIO48 on most DevKit boards)
   - Equivalent to Arduino's `RGB_BUILTIN` constant

9. **Audit Logging**
   - Syslog RFC5424 format
   - Events: auth attempts, OTA updates, SysRq usage, factory resets
   - 4KB RAM buffer (last ~100 events)
   - Persisted to NVS at reboot
   - Retrievable via PWA
   - No sensitive data logged
   - Future: remote syslog server support

10. **Mandatory PIN via profisioning flow via PWA**
   - PIN MUST be set via provisioning flow
   - No random PIN generation
   - No default PIN fallback
   - PWA validates PIN format (6 digits, not 000000)
   - User consciously chooses initial PIN

### Intended User Flow

1. **First time**: User visits `https://<username>.github.io/<repo>/` in Chrome → installs the PWA → navigates to "Flash Firmware" page → flashes firmware via Web Serial (no parameters needed) → powers on ESP32
2. **Provisioning mode (first boot)**: ESP32 boots without PIN → enters provisioning mode → broadcasts BLE service "ESP32-HID-SETUP" → built-in LED blinks orange slowly
3. **Initial setup**: Open PWA → PWA detects provisioning mode → connect via BLE → provisioning UI appears → enter 6-digit PIN (mandatory, validated) → optionally enter WiFi credentials (Improv WiFi protocol) → save → ESP32 reboots into normal mode
4. **First pairing (BLE)**: Open PWA → connect via Bluetooth → OS prompts for passkey → enter chosen PIN → PWA forces PIN change before any other action
5. **Daily use (BLE)**: Open PWA → connect via Bluetooth → paste/type text → ESP32 types it out (red LED flashes)
6. **Certificate setup (once per device)**: 
   - PWA reads certificate fingerprint via BLE
   - Download certificate from `http://<ip>/cert.pem`
   - PWA verifies fingerprint match
   - Import certificate into OS trust store
   - Restart browser → WSS works
7. **Daily use (Network)**: Open PWA → enter ESP32 IP + PIN → connect via WSS → paste/type text → ESP32 types it out
8. **WiFi config**: Via BLE, configure the ESP32 to join a WiFi network for WSS access (also possible during provisioning)
9. **Firmware update**: Via PWA "Flash Firmware" page (USB, Web Serial) or OTA update over WiFi (signed firmware only)
10. **Factory reset**: Hold BOOT button for 10 seconds → LED flashes yellow (warning) → after 10s LED turns red → device resets → back to provisioning mode (orange slow blink) → re-provision via PWA. No USB-UART or serial console needed.
11. **Advanced (Linux)**: Opt-in to SysRq menu in settings → use SysRq magic keys for kernel-level commands

## Architecture

```
┌──────────────────┐                    ┌─────────────────┐     USB HID      ┌──────────────┐
│ Preact PWA       │   BLE GATT         │   ESP32-S3      │ ──────────────►  │ Target PC    │
│ (GitHub Pages)   │ ─────────────►     │                 │   Keyboard       │ (receives    │
│                  │   Web Bluetooth    │                 │   keystrokes     │  keystrokes) │
│                  │                    │ - USB HID       │                  └──────────────┘
│                  │   WSS              │ - BLE Server    │
│                  │ ─────────────►     │ - WiFi AP/STA   │   ┌──────────────┐
│                  │   wss:// + PIN     │ - HTTPS/WSS     │   │ BOOT Button  │
└──────────────────┘                    │ - HTTP (cert)   │◄──┤ (GPIO0)      │
                                        │ - Typing Engine │   │ Factory Reset│
                                        │ - OTA Update    │   └──────────────┘
                                        │ - NeoPixel LED  │
                                        │ - Provisioning  │
                                        └─────────────────┘
```

### Key Design Decisions

- **GitHub Pages hosting**: The PWA is built and deployed via GitHub Actions. Free HTTPS, webapp updates without firmware flashing.
- **PWA-first**: Full offline support via service worker. Install once, use forever over BLE.
- **No Secure Boot / No eFuse burning**: Hardware remains fully reflashable with any software. This is intentional for an open-source project — users should always be able to repurpose their hardware. OTA updates are signed at application level.
- **ESP-IDF with idf.py**: Native ESP-IDF build system, no PlatformIO. Direct use of ESP Component Registry (`idf_component.yml`) for TinyUSB and other components.
- **DevContainer for building only**: The devcontainer provides `idf.py` and Node.js. Flashing and serial monitoring happen via the browser (Web Serial API), not from within the container. No USB passthrough needed. Works on Linux, macOS, and Windows.
- **Dual transport**: BLE (primary) and WSS (fallback). Both use the same PIN for authentication.
- **BLE LE Secure Connections**: Enforce encrypted BLE connections with passkey pairing. Reject unencrypted connections and legacy pairing. Only most recent bonding kept.
- **WSS with ECDSA self-signed certificate**: The ESP32 generates a self-signed ECDSA P-256 certificate at first boot (via mbedTLS), stored in encrypted NVS. The HTTPS server serves WSS on port 8443.
- **Certificate trust via BLE fingerprint**: Certificate SHA256 fingerprint exposed via BLE. PWA verifies fingerprint before trusting certificate downloaded via HTTP.
- **Certificate download via plain HTTP**: A plain HTTP server on port 80 serves ONLY the certificate file for download (`.crt` and `.pem`). After fingerprint verification and importing into the OS/browser trust store, WSS connections work permanently.
- **Provisioning mode on first boot**: Device boots without PIN → enters provisioning mode → broadcasts BLE service → PWA connects and shows setup UI → user sets PIN + optional WiFi. Similar to ESPHome and other ESP32 projects.
- **Improv WiFi protocol**: Optional WiFi provisioning via BLE using Improv WiFi standard (https://www.improv-wifi.com/) for compatibility with other tools.
- **WiFi config via BLE**: Scan, connect, manage saved networks — all over BLE.
- **OTA firmware updates**: The PWA can push signed firmware updates over WiFi using ESP-IDF's OTA mechanism. Dual app partitions (`ota_0` + `ota_1`) with rollback support. Signature verification is application-level (public key embedded in firmware), not Secure Boot. USB flashing is always allowed with any firmware.
- **USB HID via TinyUSB**: Installed via ESP Component Registry (`espressif/esp_tinyusb`). Native USB OTG pins: GPIO19 (D-), GPIO20 (D+).
- **Web Serial tools in PWA**: The PWA includes a firmware flash page (esptool-js) and an optional serial monitor page, both using the Web Serial API. No tools need to be installed locally beyond a Chromium browser.
- **Factory reset via BOOT button**: Press and hold BOOT button (GPIO0) for 10 seconds while device is running → LED flashes yellow rapidly as warning → after 10s LED turns red → factory reset executes → device reboots into provisioning mode. Physical access required.
- **Optional serial console**: Serial console commands available for advanced users/debugging, but not required for normal operation. Factory reset via BOOT button is the primary method.
- **SysRq magic keys**: Advanced menu (collapsed by default, opt-in toggle in settings) that sends Linux Magic SysRq key combinations via USB HID. Requires explicit user consent with 10-second cooldown and separate confirmation per action. Useful for emergency kernel operations on the target machine.
- **RGB LED status feedback**: Visual indication of connection state and typing activity. Always at 5% brightness (configurable via PWA).
- **Audit logging**: Syslog RFC5424 format, 4KB RAM buffer persisted to encrypted NVS at reboot. Retrievable via PWA. Logs auth attempts, OTA updates, SysRq usage, factory resets. No sensitive data logged.
- **NVS encryption (partition-based)**: All sensitive data (PIN, WiFi credentials, certificates, logs) encrypted in flash with keys in `nvs_keys` partition. No eFuse burning — hardware remains reflashable.

## Supported Hardware

| Chip | USB OTG Pins | BLE | WiFi | Status |
|------|-------------|-----|------|--------|
| ESP32-S3 | GPIO19 (D-), GPIO20 (D+) | ✅ | ✅ | **Supported** |
| ESP32-P4 | Per datasheet | ✅ | ✅ | Planned (future) |
| ESP32-S2 | — | ❌ | ✅ | **Not supported** (no BLE) |

**USB PHY note**: The S3 shares a single internal USB PHY between USB-OTG and USB-Serial-JTAG. When TinyUSB initializes, USB-Serial-JTAG becomes unavailable. A separate USB-UART bridge chip (e.g., CP2102, CH340) on dedicated GPIO pins is **required** for serial console access.

**Board requirement**: Any ESP32-S3 development board or custom board with:
- USB OTG pins exposed (GPIO19/GPIO20)
- Built-in WS2812 NeoPixel LED (GPIO48 on most DevKit boards, check your schematic)
- BOOT button (GPIO0, standard on all ESP32 dev boards)

**Optional (for debugging only)**:
- USB-UART bridge on dedicated TX/RX pins for serial console debugging

## Tech Stack

| Component | Technology |
|-----------|-----------|
| Firmware build | ESP-IDF v5.3+ with `idf.py` |
| USB HID | TinyUSB via `espressif/esp_tinyusb` (Component Registry) |
| BLE | NimBLE (ESP-IDF component, lighter than Bluedroid) |
| WiFi | ESP-IDF WiFi driver (AP+STA coexistence) |
| HTTPS/WSS Server | `esp_https_server` (ESP-IDF) with ECDSA self-signed cert, port 8443 |
| HTTP Server | `esp_http_server` (ESP-IDF) for certificate download, port 80 |
| TLS/Crypto | mbedTLS (bundled with ESP-IDF), ECDSA P-256 for certificates |
| OTA | `esp_https_ota` with application-level signature verification |
| NVS | Encrypted NVS with partition-based keys (no eFuse) |
| Web App | Preact + TypeScript + Vite |
| PWA | vite-plugin-pwa (Workbox) |
| BLE API | Web Bluetooth API (Chromium only) |
| Serial tools | esptool-js + Web Serial API (Chromium only) |
| Hosting | GitHub Pages (free HTTPS) |
| CI/CD | GitHub Actions |
| Dev Env | VS Code DevContainer with ESP-IDF + Node.js (build only) |

## Repository Structure

```
esp32-ble-hid-typer/
├── .devcontainer/
│   ├── devcontainer.json
│   └── Dockerfile
├── .github/
│   └── workflows/
│       ├── deploy-webapp.yml        # Build Preact app → deploy to GitHub Pages
│       └── build-firmware.yml       # Build + sign firmware → GitHub Release
├── firmware/
│   ├── CMakeLists.txt               # Top-level ESP-IDF project CMakeLists
│   ├── sdkconfig.defaults           # Common defaults
│   ├── sdkconfig.defaults.esp32s3   # S3-specific overrides
│   ├── partitions.csv               # Partition table with OTA support
│   ├── ota_signing_key.pem          # OTA signing key (gitignored, in GitHub Secrets for CI)
│   └── main/
│       ├── CMakeLists.txt           # Component CMakeLists
│       ├── idf_component.yml        # ESP Component Registry dependencies
│       ├── main.c                   # App entrypoint, provisioning vs normal mode
│       ├── provisioning.h / .c      # Provisioning mode (BLE service, commands)
│       ├── improv_wifi.h / .c       # Improv WiFi protocol implementation
│       ├── usb_hid.h / usb_hid.c   # TinyUSB HID keyboard setup & report sending
│       ├── ble_server.h / .c        # NimBLE GATT server (normal mode)
│       ├── ble_security.h / .c      # LE Secure Connections, passkey, bonding
│       ├── ble_cert_service.h / .c  # GATT service for certificate fingerprint
│       ├── wifi_manager.h / .c      # WiFi AP+STA, scan, connect, saved networks
│       ├── https_server.h / .c      # HTTPS server: WSS endpoint (port 8443)
│       ├── http_server.h / .c       # HTTP server: certificate download (port 80)
│       ├── ws_handler.h / .c        # WebSocket message handling, PIN auth
│       ├── tls_certs.h / .c         # ECDSA self-signed cert generation & NVS storage
│       ├── auth.h / .c              # PIN storage (encrypted NVS), verification, rate limiting
│       ├── ota.h / .c               # OTA update handler with app-level signature verification
│       ├── typing_engine.h / .c     # Text-to-HID-keycode conversion & throttled output
│       ├── sysrq.h / sysrq.c       # SysRq magic key combinations via HID
│       ├── serial_cmd.h / .c        # UART console command handler (factory_reset, etc.)
│       ├── button_reset.h / .c      # BOOT button long-press for factory reset (10s)
│       ├── audit_log.h / .c         # Syslog RFC5424 logging to RAM + NVS
│       ├── neopixel.h / .c          # WS2812 NeoPixel control via RMT (built-in LED GPIO48)
│       ├── perf_monitor.h / .c      # Heap/stack usage monitoring (DEBUG flag)
│       ├── keymap_us.h              # US keyboard layout HID keycode mapping
│       └── Kconfig.projbuild        # Custom menuconfig options (if needed)
├── webapp/
│   ├── package.json
│   ├── tsconfig.json
│   ├── vite.config.ts
│   ├── index.html
│   ├── public/
│   │   ├── manifest.json            # PWA manifest
│   │   └── icons/                   # PWA icons (192x192, 512x512)
│   └── src/
│       ├── index.tsx                 # Preact render entrypoint
│       ├── app.tsx                   # Main app component, routing
│       ├── components/
│       │   ├── ConnectionScreen.tsx  # Choose BLE or Network, connection status
│       │   ├── ProvisioningScreen.tsx # Initial setup: PIN + WiFi via BLE provisioning
│       │   ├── BleConnect.tsx        # BLE connect/disconnect, pairing (normal mode)
│       │   ├── NetworkConnect.tsx    # IP input, PIN entry, WSS connect
│       │   ├── CertificateSetup.tsx  # Certificate download + fingerprint verification + import instructions
│       │   ├── PinSetup.tsx          # Forced PIN change screen (blocks until changed)
│       │   ├── TextSender.tsx        # Textarea + send button
│       │   ├── ClipboardPaste.tsx    # "Paste & Send" button (navigator.clipboard)
│       │   ├── StatusBar.tsx         # Connection status, typing progress bar
│       │   ├── WifiConfig.tsx        # WiFi scan, connect, saved networks, AP settings
│       │   ├── FirmwareFlash.tsx     # Flash via Web Serial (esptool-js), no PIN parameter
│       │   ├── OtaUpdate.tsx         # OTA update over WiFi
│       │   ├── SerialMonitor.tsx     # Serial console via Web Serial (optional, for debugging)
│       │   ├── SysRqPanel.tsx        # SysRq menu (opt-in, warning dialog, cooldown)
│       │   ├── AuditLog.tsx          # View audit log from ESP32
│       │   ├── Settings.tsx          # Typing speed, layout, PIN change, SysRq opt-in, LED brightness
│       │   ├── Guide.tsx             # Setup instructions + security warnings
│       │   └── SecurityWarning.tsx   # Security information and warnings
│       ├── utils/
│       │   ├── ble.ts                # Web Bluetooth API wrapper
│       │   ├── websocket.ts          # WSS connection + message handling
│       │   ├── auth.ts               # PIN validation + rate limiting (client-side)
│       │   ├── fingerprint.ts        # Certificate fingerprint calculation & verification
│       │   └── storage.ts            # localStorage wrapper (PIN, settings, SysRq opt-in)
│       └── types/
│           └── protocol.ts           # Shared protocol types (BLE GATT UUIDs, WS messages)
├── docs/
│   ├── SECURITY.md                  # Security architecture documentation
│   ├── OTA_SIGNING.md               # OTA signing key generation and firmware signing instructions
│   ├── HARDWARE.md                  # Required hardware components
│   └── THREAT_MODEL.md              # Threat model analysis
└── README.md
```

## BLE Protocol (Normal Mode)

**Note**: This section describes the BLE protocol for normal operation mode (after provisioning). For provisioning mode BLE protocol, see the "Provisioning Mode" section above.

### Services & Characteristics

```
Service: HID Typer Service
UUID: 6e400001-b5a3-f393-e0a9-e50e24dcca9e

Characteristics:
1. Text Input (Write, Write Without Response)
   UUID: 6e400002-b5a3-f393-e0a9-e50e24dcca9e
   Max: 512 bytes
   Purpose: Send text to type

2. Status (Read, Notify)
   UUID: 6e400003-b5a3-f393-e0a9-e50e24dcca9e
   Format: JSON
   Example: {"connected":true,"typing":false,"queue":0,"pin_set":true}

3. PIN Management (Write)
   UUID: 6e400004-b5a3-f393-e0a9-e50e24dcca9e
   Format: {"action":"set","old":"123456","new":"654321"}
   Actions: set, verify

4. WiFi Config (Write, Read)
   UUID: 6e400005-b5a3-f393-e0a9-e50e24dcca9e
   Format: JSON
   Actions: scan, connect, disconnect, list, forget

5. Certificate Fingerprint (Read)
   UUID: 6e400006-b5a3-f393-e0a9-e50e24dcca9e
   Format: 64-char hex string (SHA256)
   Purpose: Verify certificate downloaded via HTTP
```

### Security

- BLE pairing requires PIN (default: set via profisioning flow via PWA)
- LE Secure Connections enforced
- Reject unencrypted or legacy pairing modes
- Only most recent bonding kept (auto-cleanup)
- Rate limiting: 3 PIN attempts per 60s, lockout after 10 failures

### BLE Message Flow

```
PWA                              ESP32
 │                                 │
 │──────── Connect ────────────────►│
 │                                 │
 │◄───── Pairing Request ──────────│
 │                                 │
 │──── Enter PIN (via OS) ─────────►│
 │                                 │
 │◄───── Read Status ──────────────│
 │                                 │
 │──── (if pin_set=false) ─────────│
 │──── Write PIN change ───────────►│
 │                                 │
 │◄───── Success ──────────────────│
 │                                 │
 │──── Read Cert Fingerprint ──────►│
 │                                 │
 │◄───── SHA256 hex ───────────────│
 │                                 │
 │──── Download cert via HTTP ─────►│
 │──── Verify fingerprint match ────│
 │                                 │
 │──── Write WiFi config ──────────►│
 │                                 │
 │◄───── WiFi connected ────────────│
 │                                 │
 │──── Write Text Input ───────────►│
 │                                 │
 │◄───── Status notifications ──────│
 │        (typing progress)         │
```

## WebSocket Protocol

### Connection

```
wss://<esp32-ip>:8443/ws

Headers:
  Authorization: PIN <6-digit-pin>
```

### Message Format (JSON)

```json
// Client → ESP32
{
  "type": "text",
  "data": "Hello, world!"
}

{
  "type": "wifi_scan",
  "data": {}
}

{
  "type": "wifi_connect",
  "data": {"ssid": "MyNetwork", "password": "secret"}
}

{
  "type": "ota_update",
  "data": {"url": "https://github.com/user/repo/releases/download/v1.0/firmware.bin"}
}

// ESP32 → Client
{
  "type": "status",
  "data": {"connected": true, "typing": false, "queue": 0}
}

{
  "type": "wifi_scan_result",
  "data": [{"ssid": "MyNetwork", "rssi": -45, "secure": true}, ...]
}

{
  "type": "typing_progress",
  "data": {"current": 50, "total": 100}
}

{
  "type": "error",
  "data": {"message": "PIN verification failed"}
}
```

### Security

- WSS (TLS 1.2+) with ECDSA self-signed certificate
- Certificate fingerprint verified via BLE before trust
- PIN authentication on every connection (Authorization header)
- Rate limiting: 3 PIN attempts per 60s, lockout after 10 failures
- Idle timeout: 5 minutes of inactivity → disconnect

## Provisioning Mode

The ESP32 enters provisioning mode automatically on first boot or after factory reset when no PIN is found in NVS. This follows the same pattern as ESPHome, Tasmota, and other ESP32 projects.

### Provisioning Mode Behavior

```c
// In main.c

void app_main(void) {
    nvs_flash_init();
    
    char pin[7];
    esp_err_t err = nvs_get_str(nvs_handle, "pin", pin, sizeof(pin));
    
    if (err != ESP_OK || strlen(pin) == 0) {
        ESP_LOGI(TAG, "No PIN found - entering provisioning mode");
        provisioning_mode_start();
        return;  // Provisioning mode runs until reboot
    }
    
    ESP_LOGI(TAG, "PIN loaded from NVS - normal mode");
    normal_mode_start();
}
```

### Provisioning Mode Indicators

- **Built-in NeoPixel LED**: Blinks orange slowly (1 second on, 1 second off)
- **BLE Service**: Broadcasts as "ESP32-HID-SETUP"
- **No USB HID**: USB HID functionality disabled in provisioning mode
- **Console Log**: "Provisioning mode active - waiting for setup via PWA"

### Provisioning BLE Service

```
Service: Provisioning Service
UUID: 00467768-6228-2272-4663-277478268000  # Improv WiFi compatible

Characteristics:
1. Status (Read, Notify)
   UUID: 00467768-6228-2272-4663-277478268001
   Format: uint8_t
   Values: 0=ready, 1=provisioning, 2=provisioned
   
2. Error (Read, Notify)
   UUID: 00467768-6228-2272-4663-277478268002
   Format: uint8_t
   Values: 0=none, 1=invalid_pin, 2=unable_to_connect, 3=unknown
   
3. RPC Command (Write)
   UUID: 00467768-6228-2272-4663-277478268003
   Format: JSON
   Purpose: Send provisioning commands
   
4. RPC Result (Read, Notify)
   UUID: 00467768-6228-2272-4663-277478268004
   Format: JSON
   Purpose: Receive command results
```

### Provisioning Commands (via RPC)

```json
// Set PIN (mandatory)
{
  "command": "set_pin",
  "pin": "123456"
}

// Response:
{
  "success": true,
  "message": "PIN set successfully"
}

// Set WiFi (optional, Improv WiFi compatible)
{
  "command": "set_wifi",
  "ssid": "MyNetwork",
  "password": "secret123"
}

// Response:
{
  "success": true,
  "message": "WiFi credentials saved",
  "ip_address": "192.168.1.100"
}

// Complete provisioning (reboot into normal mode)
{
  "command": "complete"
}

// Response:
{
  "success": true,
  "message": "Provisioning complete, rebooting..."
}
```

### PWA Provisioning UI Flow

```typescript
// In ProvisioningScreen.tsx

1. Detect provisioning mode:
   - Scan for BLE device name "ESP32-HID-SETUP"
   - Check provisioning status characteristic (0=ready)
   
2. Show provisioning UI:
   - PIN input field (6 digits, required)
   - PIN confirmation field
   - WiFi SSID input (optional)
   - WiFi password input (optional)
   - "Complete Setup" button
   
3. Validate PIN:
   - Exactly 6 digits
   - Not "000000"
   - Not sequential (123456, 654321)
   - Not repetitive (111111, 222222)
   - Confirmation matches
   
4. Send provisioning commands:
   await ble.write(RPC_COMMAND, {command: "set_pin", pin: pin});
   if (wifi_provided) {
     await ble.write(RPC_COMMAND, {command: "set_wifi", ssid, password});
   }
   await ble.write(RPC_COMMAND, {command: "complete"});
   
5. Show success:
   "Setup complete! Device is rebooting..."
   "Reconnect via normal BLE mode"
```

### Firmware Provisioning Implementation

```c
// In provisioning.c

static void provisioning_mode_start(void) {
    // Init BLE with provisioning service
    ble_provisioning_init();
    
    // Start LED blink task (orange slow blink)
    xTaskCreate(provisioning_led_task, "prov_led", 2048, NULL, 5, NULL);
    
    // Wait for commands
    while (provisioning_active) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Reboot into normal mode
    esp_restart();
}

static void provisioning_led_task(void *pvParameters) {
    while (provisioning_active) {
        neopixel_set_color(255, 165, 0, 5);  // Orange, 5% brightness
        vTaskDelay(pdMS_TO_TICKS(1000));
        neopixel_off();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

static void handle_set_pin_command(const char *pin) {
    // Validate PIN
    if (strlen(pin) != 6) {
        send_error(ERROR_INVALID_PIN);
        return;
    }
    if (strcmp(pin, "000000") == 0) {
        send_error(ERROR_INVALID_PIN);
        return;
    }
    
    // Save to encrypted NVS
    nvs_set_str(nvs_handle, "pin", pin);
    nvs_commit(nvs_handle);
    
    send_success("PIN set successfully");
}

static void handle_set_wifi_command(const char *ssid, const char *password) {
    // Save to encrypted NVS
    nvs_set_str(nvs_handle, "wifi_ssid", ssid);
    nvs_set_str(nvs_handle, "wifi_password", password);
    nvs_commit(nvs_handle);
    
    // Try to connect
    wifi_connect(ssid, password);
    
    if (wifi_is_connected()) {
        char ip[16];
        wifi_get_ip(ip, sizeof(ip));
        send_success_with_ip("WiFi connected", ip);
    } else {
        send_error(ERROR_UNABLE_TO_CONNECT);
    }
}

static void handle_complete_command(void) {
    // Verify PIN is set
    char pin[7];
    if (nvs_get_str(nvs_handle, "pin", pin, sizeof(pin)) != ESP_OK) {
        send_error(ERROR_INVALID_PIN);
        return;
    }
    
    send_success("Provisioning complete, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    provisioning_active = false;
}
```

### Improv WiFi Compatibility

The provisioning protocol is compatible with Improv WiFi (https://www.improv-wifi.com/), allowing other Improv-compatible tools to provision WiFi credentials. The PIN setup is an additional step specific to this project.

Improv WiFi defines a standard BLE service for WiFi provisioning that works across multiple projects (ESPHome, Tasmota, etc.). By implementing the Improv WiFi UUIDs and command format, this project can be provisioned by:
- The custom PWA (with PIN + WiFi)
- Generic Improv WiFi apps (WiFi only, PIN must be set separately)
- Browser-based Improv WiFi tools

**Improv WiFi Standard Characteristics:**
- Status: Reports provisioning state (ready, provisioning, provisioned)
- Error: Reports errors during provisioning
- RPC Command: Send WiFi credentials
- RPC Result: Receive results (including IP address)

**Extension for PIN Setup:**
This project extends Improv WiFi with a PIN setup command that must be sent before `set_wifi`. The `complete` command reboots the device into normal mode.

**Improv WiFi Tool Compatibility:**
- Tools that only send WiFi credentials will work, but device will remain in provisioning mode until PIN is set
- Custom PWA handles both PIN and WiFi in correct order
- Future: Add fallback AP mode with captive portal for non-BLE devices

### Factory Reset → Provisioning

After factory reset (via serial console or BOOT button):
1. PIN wiped from NVS
2. WiFi credentials wiped from NVS
3. Device reboots
4. No PIN found → provisioning mode
5. LED blinks orange slowly (1s on, 1s off)
6. User re-provisions via PWA

## BLE Protocol (Normal Mode)

1. **ESP32 Boot**
   - Generate ECDSA P-256 certificate (if not exists)
   - Store in encrypted NVS
   - Calculate SHA256 fingerprint
   - Print fingerprint to serial console

2. **PWA via BLE**
   - Read fingerprint from BLE characteristic `6e400006`
   - Display fingerprint to user

3. **PWA Download**
   - HTTP GET `http://<esp32-ip>/cert.pem`
   - Calculate SHA256 of downloaded certificate
   - Compare with BLE fingerprint

4. **Verification**
   - If match: show green checkmark + import instructions
   - If mismatch: show red warning, abort, suggest factory reset

5. **Import**
   - User imports `.pem` into OS trust store
   - macOS: Keychain Access
   - Windows: certmgr.msc
   - Linux: `/usr/local/share/ca-certificates/`
   - Restart browser

6. **WSS Connection**
   - PWA connects via `wss://<esp32-ip>:8443/ws`
   - Browser trusts certificate (no warning)

## Firmware OTA Update Flow

### USB Flash (Always Allowed)

```
User (via Web Serial flasher in PWA)
  │
  │──── Flash firmware.bin ──────────────►│ ESP32
  │                                       │
  │◄──── Boot ────────────────────────────│
  │                                       │
```

USB flashing is always allowed with any firmware. No Secure Boot, no eFuse restrictions. Hardware remains fully reflashable — this is intentional for an open-source project.

### OTA Update (Signed, Phase 3)

```
User (via PWA)
  │
  │──── Request OTA update ───────────►│ ESP32
  │                                    │
  │                                    │──── Download firmware ──────►│ GitHub
  │                                    │◄─── firmware-signed.bin ─────│
  │                                    │
  │                                    │──── Verify signature ────────│
  │                                    │  (app-level, embedded       │
  │                                    │   public key in firmware)   │
  │                                    │                              │
  │                                    │──── (if valid) ──────────────│
  │                                    │──── Flash to ota_1 ──────────│
  │                                    │──── Set boot partition ──────│
  │                                    │──── Reboot ──────────────────│
  │◄─── Booted into new firmware ──────│
  │                                    │
  │──── (if app crashes) ──────────────│
  │◄─── Rollback to ota_0 ─────────────│
```

### OTA Signing Key Generation

```bash
# Generate ECDSA P-256 signing key pair (do this once, keep private key secret)
openssl ecparam -genkey -name prime256v1 -out ota_signing_key.pem
openssl ec -in ota_signing_key.pem -pubout -out ota_signing_pubkey.pem

# The public key (ota_signing_pubkey.pem) is embedded in the firmware source
# The private key (ota_signing_key.pem) is stored as a GitHub Secret for CI

# Sign firmware for OTA distribution
openssl dgst -sha256 -sign ota_signing_key.pem -out firmware.sig firmware.bin
```

See `docs/OTA_SIGNING.md` for detailed instructions.

## Typing Engine

### Text-to-Keycode Conversion

```c
// Input: UTF-8 string
// Output: Stream of HID reports

typedef struct {
    uint8_t keycode;
    uint8_t modifier;  // Left Shift, Right Shift, etc.
} hid_key_t;

hid_key_t keymap_us[128] = {
    ['a'] = { 0x04, 0x00 },
    ['A'] = { 0x04, 0x02 },  // Left Shift
    ['1'] = { 0x1E, 0x00 },
    ['!'] = { 0x1E, 0x02 },  // Left Shift
    [' '] = { 0x2C, 0x00 },
    ['\n'] = { 0x28, 0x00 }, // Enter
    ['\t'] = { 0x2B, 0x00 }, // Tab
    // ... complete mapping for all printable ASCII
};
```

### Rate Limiting

- Configurable delay between keystrokes (default: 10ms)
- Configurable delay between words (default: 50ms)
- Maximum rate: 1000 chars/minute (safety limit)
- User adjustable via PWA settings (5ms - 100ms)

### Queue Management

- Text queued in RAM (max 8KB)
- Typed character-by-character
- Progress notifications sent via BLE/WSS
- Abort command to clear queue

## RGB LED Status Codes

| State | Color | Pattern | Meaning |
|-------|-------|---------|---------|
| Provisioning Mode | Orange | Slow blink (1s on, 1s off) | Waiting for initial setup via PWA |
| Disconnected | Off | Solid | No BLE or WSS connection (normal mode) |
| BLE Connected | Blue | Solid | BLE connected, idle |
| WiFi Connected | White | Solid | WiFi connected, no WSS |
| WSS Connected | Yellow | Solid | WSS connected, idle |
| Typing | Red | Flashing (500ms on/off) | Actively typing |
| Factory Reset Warning | Yellow | Rapid flash (100ms on/off) | BOOT button held 2-10s, release to cancel |
| Factory Reset Confirmed | Red | Solid (1s) | BOOT button held 10s, resetting now |
| Error | Red | Rapid blink (100ms on/off) | Error state (no PIN after normal boot attempt) |
| OTA Update | Purple | Pulsing | Firmware update in progress |

- All at 5% brightness (configurable via PWA: 1-100%)
- Hardware: ESP32-S3 built-in WS2812 NeoPixel LED
- GPIO48 on most ESP32-S3 DevKit boards (check your board schematic)
- Arduino equivalent: `RGB_BUILTIN`
- ESP-IDF: Use RMT peripheral or `led_strip` component from ESP Component Registry
- Example: `espressif/led_strip` component for WS2812 control

### NeoPixel Implementation (ESP-IDF)

```c
// In idf_component.yml:
dependencies:
  espressif/led_strip: "^2.5.0"

// In neopixel.c:
#include "led_strip.h"

#define LED_STRIP_GPIO 48  // Built-in NeoPixel on ESP32-S3 DevKit
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)  // 10MHz

static led_strip_handle_t led_strip;

void neopixel_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

void neopixel_set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness_percent) {
    // Apply brightness (0-100%)
    r = (r * brightness_percent) / 100;
    g = (g * brightness_percent) / 100;
    b = (b * brightness_percent) / 100;
    
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

void neopixel_off(void) {
    led_strip_clear(led_strip);
}

// LED state functions
void neopixel_provisioning_mode(void) {
    neopixel_set_color(255, 165, 0, 5);  // Orange, 5% brightness
}

void neopixel_ble_connected(void) {
    neopixel_set_color(0, 0, 255, 5);    // Blue, 5% brightness
}

void neopixel_wifi_connected(void) {
    neopixel_set_color(255, 255, 255, 5); // White, 5% brightness
}
```

## Audit Logging

### Log Format (Syslog RFC5424)

```
<priority>version timestamp hostname app-name proc-id msg-id structured-data msg

Example:
<134>1 2024-02-08T12:00:00.000Z esp32-hid - auth_attempt - - transport=ble result=fail reason=invalid_pin
<134>1 2024-02-08T12:05:00.000Z esp32-hid - ota_start - - version=v1.2.0 transport=wss
<134>1 2024-02-08T12:10:00.000Z esp32-hid - sysrq_exec - - key=h target=help
<134>1 2024-02-08T12:15:00.000Z esp32-hid - factory_reset - - trigger=serial
```

### Storage

- 4KB RAM buffer (ring buffer, ~100 events)
- At reboot: persist to encrypted NVS
- At boot: load from NVS → RAM
- Retrievable via PWA (BLE or WSS)
- PWA can display, export, or clear logs

### Logged Events

- Authentication attempts (success/fail, transport, reason)
- OTA updates (start, success, fail, version)
- SysRq executions (key, target)
- Factory resets (trigger)
- Certificate regeneration
- PIN changes
- WiFi connections/disconnections
- No sensitive data (no PINs, passwords, typed text)

### Future: Remote Syslog

- Configurable syslog server (IP/hostname)
- TLS-encrypted syslog (RFC5425)
- Automatic push on event (if WiFi connected)
- Fallback to local NVS if server unreachable

## Performance Monitoring (DEBUG only)

```c
#ifdef DEBUG_PERF_MONITOR
void perf_monitor_task(void *pvParameters) {
    while (1) {
        ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "Min free heap: %d bytes", esp_get_minimum_free_heap_size());
        ESP_LOGI(TAG, "Largest free block: %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        
        // Stack high water mark for each task
        ESP_LOGI(TAG, "USB task stack: %d bytes", uxTaskGetStackHighWaterMark(usb_task_handle));
        ESP_LOGI(TAG, "BLE task stack: %d bytes", uxTaskGetStackHighWaterMark(ble_task_handle));
        ESP_LOGI(TAG, "WSS task stack: %d bytes", uxTaskGetStackHighWaterMark(wss_task_handle));
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Every 10 seconds
    }
}
#endif
```

Enable via `idf.py menuconfig` → Component config → Heap memory debugging → Enable.

## SysRq Implementation

### GATT Characteristic

```
UUID: 6e400007-b5a3-f393-e0a9-e50e24dcca9e
Write: {"action":"sysrq","key":"h"}
```

### WebSocket Message

```json
{
  "type": "sysrq",
  "data": {"key": "h"}
}
```

### HID Sequence

```c
// SysRq = Alt + PrintScreen + <key>
void send_sysrq(char key) {
    // Press Alt
    send_modifier(0x04);  // Left Alt
    // Press PrintScreen
    send_keycode(0x46);   // Print Screen
    // Press key
    send_keycode(keymap[key].keycode);
    // Release all
    send_empty_report();
}
```

### Supported Keys

| Key | Action | Description |
|-----|--------|-------------|
| h | Help | Display help |
| b | Reboot | Immediate reboot (no sync) |
| c | Crash | Trigger kernel crash dump |
| d | Display | Show all held locks |
| e | Terminate | Send SIGTERM to all processes |
| f | OOM Kill | Call OOM killer |
| i | Kill | Send SIGKILL to all processes |
| k | SAK | Secure Access Key (kill all on console) |
| m | Memory | Show memory info |
| n | Nice | Reset RT tasks to normal priority |
| o | Poweroff | Shutdown |
| p | Registers | Show registers |
| q | Timers | Show per-CPU timer lists |
| r | Keyboard | Take keyboard out of raw mode |
| s | Sync | Sync all mounted filesystems |
| t | Tasks | Show task list |
| u | Remount | Remount all filesystems read-only |
| v | ETM | ARM/ARM64 ETM buffer dump |
| w | Blocked | Show blocked tasks |
| z | ftrace | Dump ftrace buffer |

### REISUB Sequence

For safe reboot of a frozen Linux system:

```
R - Take keyboard out of raw mode
E - Send SIGTERM to all processes
I - Send SIGKILL to all processes
S - Sync all filesystems
U - Remount all filesystems read-only
B - Reboot
```

PWA implements a "Safe Reboot (REISUB)" button that sends these keys with 2-second delays.

### Safety Controls

1. **Settings Opt-In**
   - SysRq menu hidden by default
   - "Enable SysRq" toggle in settings
   - Opt-in stored in localStorage (per browser/device)
   - Toggle shows warning about risks

2. **Confirmation Dialog**
   - Every SysRq action requires confirmation
   - Dialog shows key, action, and description
   - "I understand this is dangerous" checkbox
   - 10-second cooldown (button disabled)
   - Separate confirmation per key (no "always allow")

3. **Audit Logging**
   - Every SysRq execution logged
   - Log includes: timestamp, key, transport

## Serial Console Commands (Optional)

**Note**: Serial console access is **optional** and primarily for debugging. All essential functionality (firmware flashing, provisioning, factory reset) is available via the PWA and BOOT button. Users do not need a USB-UART bridge for normal operation.

### Protocol

Commands sent via UART serial console (115200 baud, 8N1):

```
<command> [args]\n
```

### Supported Commands

| Command | Args | Description |
|---------|------|-------------|
| `status` | None | Show connection status, WiFi, IP, uptime, heap usage |
| `cert_fingerprint` | None | Display certificate SHA256 fingerprint |
| `factory_reset` | None | Same as BOOT button 10s hold - reboot to provisioning mode |
| `full_reset` | None | Factory reset + regenerate certificate + wipe bonding |
| `reboot` | None | Reboot device |
| `heap` | None | Show detailed heap usage statistics |
| `wifi_scan` | None | Scan for WiFi networks and display results |
| `help` | None | Show command list |

**Note**: `factory_reset` via serial is equivalent to BOOT button method. `full_reset` additionally regenerates the certificate (requires certificate fingerprint re-verification).

### Factory Reset via BOOT Button

**Primary method**: Press and hold BOOT button while device is running.

**Advantages over serial console:**
- ✅ No USB-UART bridge required
- ✅ No serial terminal software needed
- ✅ Visual LED feedback (yellow warning → red confirm)
- ✅ Cancellable (release before 10s)
- ✅ Works on any ESP32-S3 dev board (BOOT button is standard)
- ✅ Physical access required (security)
- ✅ User-friendly (no commands to type)

**LED Feedback Pattern:**
1. **0-2 seconds**: Normal operation LED (blue/white/yellow depending on state)
2. **2-10 seconds**: Yellow rapid flash (warning - reset will trigger soon)
3. **10 seconds**: Red solid for 1 second (reset confirmed)
4. **After 10s**: Factory reset executes, device reboots into provisioning mode (orange slow blink)

**If button released before 10 seconds**: Reset cancelled, LED returns to normal state.

**Technical Implementation:**
```c
// In button_reset.c

void boot_button_monitor_task(void *pvParameters) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_0),  // GPIO0 = BOOT button
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
    
    uint32_t press_start = 0;
    bool was_pressed = false;
    
    while (1) {
        bool is_pressed = (gpio_get_level(GPIO_NUM_0) == 0);  // Active low
        
        if (is_pressed && !was_pressed) {
            press_start = xTaskGetTickCount();
            ESP_LOGI(TAG, "BOOT button pressed - hold for 10s to factory reset");
        }
        
        if (is_pressed) {
            uint32_t duration_ms = (xTaskGetTickCount() - press_start) * portTICK_PERIOD_MS;
            
            if (duration_ms >= 2000 && duration_ms < 10000) {
                // Warning: yellow rapid flash
                neopixel_flash(255, 255, 0, 100);  // Yellow, 100ms period
            }
            
            if (duration_ms >= 10000 && !reset_triggered) {
                // Trigger reset
                ESP_LOGW(TAG, "Factory reset triggered via BOOT button");
                neopixel_set_color(255, 0, 0, 100);  // Red solid
                vTaskDelay(pdMS_TO_TICKS(1000));
                
                factory_reset();
                esp_restart();
            }
        }
        
        if (!is_pressed && was_pressed) {
            ESP_LOGI(TAG, "BOOT button released - reset cancelled");
            neopixel_restore_normal_state();
        }
        
        was_pressed = is_pressed;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Factory Reset Behavior

- Wipes PIN from NVS
- Wipes WiFi credentials
- Wipes AP settings
- Keeps certificate (use `full_reset` via serial for complete wipe)
- Does NOT wipe bonding database
- Reboots into provisioning mode
- Available via BOOT button (primary) OR serial console command (optional)
- After reset: device broadcasts "ESP32-HID-SETUP" with orange slow blinking LED → re-provision via PWA

### Serial Console Commands (Optional)

Serial console is **optional** for advanced users and debugging. The BOOT button provides all necessary reset functionality for normal users.

Commands available via serial console (115200 baud, 8N1):

### Full Reset (Serial Console Only)

- Everything in factory reset (PIN, WiFi, AP settings wiped)
- Regenerates ECDSA certificate (new fingerprint!)
- Wipes bonding database
- Wipes audit log
- Reboots into provisioning mode
- **Requires certificate re-verification** after reprovisioning (new fingerprint)
- Only available via serial console (not BOOT button)
- Use case: Certificate compromised or annual security refresh

## GitHub Actions

### deploy-webapp.yml

```yaml
name: Deploy Webapp
on:
  push:
    branches: [main]
    paths: ['webapp/**']

permissions:
  contents: read
  pages: write
  id-token: write

jobs:
  deploy:
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
      - run: cd webapp && npm ci && npm run build
      - uses: actions/upload-pages-artifact@v3
        with:
          path: webapp/dist
      - id: deployment
        uses: actions/deploy-pages@v4
```

### build-firmware.yml

```yaml
name: Build Firmware
on:
  push:
    tags: ['v*']

jobs:
  build:
    runs-on: ubuntu-latest
    container:
      image: espressif/idf:v5.3
    steps:
      - uses: actions/checkout@v4
      
      - name: Build firmware
        run: |
          cd firmware
          idf.py set-target esp32s3
          idf.py build
      
      - name: Sign firmware for OTA
        env:
          OTA_SIGNING_KEY: ${{ secrets.OTA_SIGNING_KEY }}
        run: |
          echo "$OTA_SIGNING_KEY" > ota_signing_key.pem
          openssl dgst -sha256 -sign ota_signing_key.pem \
              -out firmware/build/firmware.sig \
              firmware/build/esp32-ble-hid-typer.bin
          rm ota_signing_key.pem

      - name: Rename binaries
        run: |
          cp firmware/build/esp32-ble-hid-typer.bin firmware-esp32s3.bin
          cp firmware/build/firmware.sig firmware-esp32s3.sig
          cp firmware/build/bootloader/bootloader.bin bootloader-esp32s3.bin
          cp firmware/build/partition_table/partition-table.bin partition-table-esp32s3.bin

      - uses: actions/upload-artifact@v4
        with:
          name: firmware-esp32s3
          path: |
            firmware-esp32s3.bin
            firmware-esp32s3.sig
            bootloader-esp32s3.bin
            partition-table-esp32s3.bin

  release:
    needs: build
    runs-on: ubuntu-latest
    permissions:
      contents: write
    steps:
      - uses: actions/download-artifact@v4
      - uses: softprops/action-gh-release@v2
        with:
          files: firmware-*/*
```

## Build Commands

```bash
# In devcontainer:

# Build firmware for ESP32-S3
cd firmware
idf.py set-target esp32s3
idf.py build

# Build webapp (dev)
cd webapp && npm run dev

# Build webapp (production)
cd webapp && npm run build

# Sign firmware for OTA (manual)
openssl dgst -sha256 -sign ota_signing_key.pem -out build/firmware.sig build/esp32-ble-hid-typer.bin
```

## Testing Strategy

- **USB HID**: `lsusb` or Device Manager to verify enumeration, text editor to verify typed output
- **Built-in NeoPixel LED**: Verify colors match connection states, orange slow blink in provisioning mode, red flashing during typing, brightness at 5%
- **Provisioning mode**: Flash firmware, verify device enters provisioning mode (orange slow blink, "ESP32-HID-SETUP" broadcast)
- **Provisioning UI**: Connect via PWA, verify PIN validation (6 digits, not 000000, sequential, repetitive)
- **Provisioning flow**: Set PIN + WiFi, verify device reboots into normal mode, verify WiFi connection
- **Web Serial flasher**: Test firmware flash without parameters
- **BLE normal mode**: nRF Connect mobile app for GATT testing after provisioning
- **BLE Security**: verify pairing requires user-chosen PIN from provisioning, verify forced PIN change on first connection
- **BLE Fingerprint**: verify reading characteristic returns 64-char hex SHA256
- **WiFi**: verify connection with credentials from provisioning, verify IP
- **Certificate generation**: verify ECDSA P-256 cert created, verify fingerprint matches across BLE and HTTP
- **HTTPS/WSS**: download cert from HTTP port, verify fingerprint via PWA, import in OS, restart browser, connect via WSS
- **Certificate verification**: test fingerprint mismatch (manual cert replacement), verify PWA rejects
- **OTA unsigned**: verify first flash via Web Serial works without signature
- **OTA signed**: verify OTA update requires valid signature, verify rollback on invalid signature
- **Audit logging**: verify events logged, retrieve via PWA, verify Syslog format, verify persistence across reboot
- **Rate limiting**: verify lockout after 10 failed PIN attempts, verify exponential backoff
- **Factory reset BOOT button**: 
  - Hold BOOT button for 2 seconds → verify yellow rapid flash (warning)
  - Release before 10s → verify LED returns to normal (cancel works)
  - Hold for full 10s → verify red solid (1s) → verify device reboots into provisioning mode (orange slow blink)
  - Verify device broadcasts "ESP32-HID-SETUP" after reset
  - Re-provision and verify normal operation
- **Factory reset serial (optional)**: send `factory_reset` via serial monitor, verify same behavior as BOOT button
- **Full reset (serial only)**: send `full_reset`, verify new certificate generated, fingerprint changes, provisioning mode
- **SysRq**: verify opt-in toggle in settings, verify confirmation dialog with cooldown, test SysRq+h on Linux
- **REISUB**: test full sequence, verify each step executes with 2s delay
- **Settings**: change typing delay, verify speed changes. Change LED brightness, verify.
- **Performance**: monitor heap usage during operation (DEBUG flag), verify no memory leaks
- **NVS encryption**: verify PIN is encrypted in NVS (partition-based encryption)
- **Improv WiFi**: test provisioning with generic Improv WiFi tool (optional)
- **Webapp BLE**: Chrome DevTools > Bluetooth
- **Webapp PWA**: Chrome DevTools > Application > Service Workers + Manifest
- **End-to-end**: complete provisioning (orange blink) → normal mode → text sending → factory reset → re-provision

## Implementation Order

Execute phases in this order, testing each before moving on:

1. **DevContainer setup** — ESP-IDF + Node.js working in container, verify `idf.py build`
2. **Firmware Phase 1** — USB HID keyboard, type hardcoded text on boot
3. **Firmware Phase 2** — Built-in NeoPixel LED control via RMT (basic colors)
4. **Firmware Phase 3** — NVS encrypted storage initialization
5. **Firmware Phase 4** — Provisioning mode detection (no PIN → provisioning mode)
6. **Firmware Phase 5** — BLE provisioning service (Improv WiFi compatible UUIDs)
7. **Firmware Phase 6** — Provisioning commands handler (set_pin, set_wifi, complete)
8. **Firmware Phase 7** — LED blink pattern for provisioning mode (orange slow blink)
9. **Webapp Phase 1** — Preact PWA shell with firmware flasher (Web Serial, no parameters)
10. **Webapp Phase 2** — Provisioning screen (detect "ESP32-HID-SETUP", show setup UI)
11. **Webapp Phase 3** — Provisioning UI: PIN input + validation + WiFi (optional)
12. **Webapp Phase 4** — Send provisioning commands via BLE, handle responses
13. **Firmware Phase 8** — Normal mode initialization (PIN loaded, start all services)
14. **Firmware Phase 9** — BLE GATT server with LE Secure Connections (normal mode)
15. **Firmware Phase 10** — PIN storage/verification in encrypted NVS, rate limiting
16. **Firmware Phase 11** — PIN management via BLE, forced change flow
17. **Webapp Phase 5** — BLE connect screen (normal mode), PIN pairing flow
18. **Webapp Phase 6** — PIN setup screen (forced change after first connection)
19. **Webapp Phase 7** — Text sending, progress, NeoPixel LED feedback
20. **Firmware Phase 12** — ECDSA certificate generation, fingerprint calculation
21. **Firmware Phase 13** — BLE certificate fingerprint service
22. **Webapp Phase 8** — Certificate fingerprint verification flow (download + compare)
23. **Firmware Phase 14** — WiFi manager, BLE WiFi config service
24. **Webapp Phase 9** — WiFi configuration UI
25. **Firmware Phase 15** — HTTPS server + WSS + HTTP cert server
26. **Webapp Phase 10** — WSS transport with certificate download/verification
27. **Firmware Phase 16** — Audit logging (Syslog format, RAM + NVS)
28. **Webapp Phase 11** — Audit log viewer
29. **Webapp Phase 12** — Settings page (typing speed, PIN change, LED brightness, SysRq opt-in)
30. **Firmware Phase 17** — BOOT button factory reset with LED feedback (yellow warning, red confirm)
31. **Firmware Phase 18** — Serial console commands (optional, for debugging)
32. **Webapp Phase 13** — Serial monitor page (optional, for debugging)
33. **Firmware Phase 19** — SysRq magic keys
33. **Webapp Phase 14** — SysRq panel (opt-in, confirmation, cooldown)
34. **Firmware Phase 19** — OTA update handler with signature verification
35. **Webapp Phase 15** — OTA update page
36. **Webapp Phase 16** — Security information and warnings
37. **Firmware Phase 20** — Performance monitoring (DEBUG flag)
38. **Firmware Phase 21** — Improv WiFi protocol full compliance (optional)
39. **Webapp Phase 17** — Guide, theme, polish
40. **GitHub Actions** — webapp deploy, firmware build + sign
41. **Documentation** — Security docs, OTA signing instructions, threat model
42. **End-to-end testing** — full flow including provisioning, normal operation, factory reset

## Security Checklist

Before releasing to production:

- [ ] Provisioning mode triggers automatically on first boot (no PIN in NVS)
- [ ] Provisioning BLE service broadcasts "ESP32-HID-SETUP"
- [ ] Provisioning LED indicator (orange slow blink) working
- [ ] Provisioning UI validates PIN format (6 digits, not 000000, not sequential/repetitive)
- [ ] Provisioning commands (set_pin, set_wifi, complete) working
- [ ] Normal mode only starts after PIN is set
- [ ] PIN change forced on first BLE connection (normal mode)
- [ ] Rate limiting active (3 attempts/60s, lockout after 10)
- [ ] BLE LE Secure Connections enforced
- [ ] Certificate fingerprint exchange via BLE working
- [ ] PWA verifies fingerprint before trust
- [ ] ECDSA P-256 certificates
- [ ] NVS encryption enabled with partition-based keys (no eFuse)
- [ ] OTA signature verification working (application-level, embedded public key)
- [ ] Built-in NeoPixel LED feedback during typing (red flashing)
- [ ] LED factory reset warning pattern (yellow rapid flash 2-10s)
- [ ] SysRq behind opt-in toggle with confirmation
- [ ] Audit logging active (Syslog format, encrypted NVS)
- [ ] Factory reset via BOOT button (10s hold) working with LED feedback
- [ ] Factory reset returns to provisioning mode (not broken state)
- [ ] Serial console optional (all essential functions work without it)
- [ ] No sensitive data in logs
- [ ] Threat model documented
- [ ] Security review completed
- [ ] Improv WiFi protocol compatibility (optional WiFi provisioning)

## Known Limitations

1. **Browser dependency**: Web Bluetooth and Web Serial only work in Chromium browsers (Chrome, Edge, Opera). Firefox and Safari not supported.

2. **No hardware-level flash protection**: No Secure Boot and no eFuse key burning — this is intentional so hardware remains fully reflashable. OTA updates are signed at application level to prevent malicious OTA pushes.

3. **Certificate trust**: User must manually import certificate into OS trust store. No way around this for self-signed certs.

4. **BLE range**: Typical range 10-30m depending on environment. Use WiFi/WSS for longer range.

5. **Typing speed**: Limited by HID report rate and USB polling. Not suitable for typing large documents (use copy/paste on target machine).

6. **Flash wear**: Even with NVS wear leveling, frequent writes will eventually wear out flash. Audit logging is designed to minimize writes.

7. **Physical access**: If attacker has physical access to the ESP32, NVS encryption keys can be read from the `nvs_keys` partition (no eFuse protection). Physical access also allows flashing arbitrary firmware. This is a conscious trade-off to keep hardware reflashable for an open-source project.

8. **Certificate rotation**: No automatic certificate rotation. User can trigger `full_reset` via serial console to regenerate cert (wipes all settings, returns to provisioning mode). For normal factory reset without certificate regeneration, use BOOT button.

9. **Provisioning security**: During provisioning mode, BLE service is open (no pairing required) to allow initial setup. After provisioning completes, device reboots into secure normal mode with PIN-protected BLE.

10. **Serial console optional**: Serial console is optional for debugging and advanced commands (`full_reset`, `heap`, `wifi_scan`). All essential user functions work without serial console access.

## Future Enhancements

- ESP32-P4 support (requires TinyUSB verification)
- Remote syslog server (TLS-encrypted)
- Multiple keyboard layouts (DE, NL, FR, etc.)
- Text snippets library (stored encrypted in NVS)
- OTP token generator (TOTP)
- Password generator
- Certificate auto-rotation (annual)
- Web-based initial setup (replace Web Serial PIN parameter with captive portal)
- E-ink display for initial PIN (no serial required)
- Hardware security module (HSM) for key storage (ESP32-C6/H2 with secure element)

## License

MIT License - see LICENSE file

## Contributing

Pull requests welcome. For major changes, please open an issue first to discuss.

## Support

- GitHub Issues: Bug reports and feature requests
- Discussions: Q&A and community support
- Security issues: Email security@example.com (private disclosure)


---

## Connectivity Behaviour (BLE + WebSocket Control Model)

### Design Goals
- Device is an operator tool, not a permanently attached keyboard
- Minimal friction for typing via PWA
- Reduce attack surface when multiple transports are available
- User-configurable security vs convenience trade-offs

### Default Behaviour

By default the device uses a **single active control transport** model:

1. BLE is enabled for connection and typing.
2. When a WebSocket (WiFi) control session becomes active:
   - BLE is automatically disabled.
   - All typing continues via WebSocket.
3. When the WebSocket disconnects:
   - BLE is automatically re-enabled.

### User‑Configurable Options (PWA Settings)

The PWA exposes the following settings, stored in encrypted NVS:

- Disable BLE when WiFi is connected (default: ON)
- Permanently disable BLE
- Enable BLE when WiFi is not connected (default: ON)

### Runtime Behaviour

The device runs a connectivity manager responsible for BLE state:

```c
void update_ble_state() {
    if (config.ble_force_disabled) {
        ble_stop();
        return;
    }

    if (wifi_ws_connected && config.ble_auto_disable_on_wifi) {
        ble_stop();
        return;
    }

    if (!wifi_connected && config.ble_enable_when_no_wifi) {
        ble_start();
        return;
    }
}
```

### Security Rationale

This model:
- Prevents simultaneous BLE + WiFi command channels
- Reduces attack surface while device is actively used
- Keeps UX friction‑free (no arming timers)
- Allows user‑chosen risk tolerance

All typing commands must remain signed and authenticated.


Read FEATURES.md
Read PLAN.md -> Phase 3 must not be implemented as of now.