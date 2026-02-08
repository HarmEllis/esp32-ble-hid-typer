# ESP32 BLE HID Typer — Execution Plan for Claude Code

This plan describes how Claude Code builds and validates the project in three phases. Each step must be completed and tested before moving to the next. **This plan intentionally contains no code** — it describes *what* needs to happen and *how to validate*, so Claude Code can implement based on the specification in `CLAUDE_updated.md`.

---

## Phase 1 — Development Environment, Tooling & CI/CD

**Goal:** A fully working monorepo with DevContainer, build tooling for both the Preact PWA and ESP-IDF firmware, and GitHub Actions for automated deployment and releases.

### 1.1 Create repository structure

- Create the full directory structure as defined in the specification (`firmware/`, `webapp/`, `docs/`, `.devcontainer/`, `.github/workflows/`).
- Add a `README.md`, `LICENSE` (MIT), and `.gitignore` (with rules for ESP-IDF build output, `node_modules`, `ota_signing_key.pem`, local sdkconfig files).

**Validation:** Directory structure matches the specification. `.gitignore` contains all required entries.

### 1.2 DevContainer configuration

- Create a `Dockerfile` based on the official `espressif/idf:v5.3` image.
- Add Node.js 20 LTS to the image (via apt or nvm).
- Create `devcontainer.json` with:
  - Reference to the Dockerfile.
  - VS Code extensions: ESP-IDF, Prettier, ESLint.
  - `postCreateCommand` that verifies both `idf.py --version` and `node --version`.
  - Working directory set to the repo root.
- **No USB passthrough** — flashing and serial monitoring happen via the browser (Web Serial API).

**Validation:** Container builds successfully. `idf.py --version` shows ESP-IDF v5.3+. `node --version` shows v20.x. `npm --version` is available.

### 1.3 Firmware build skeleton (ESP-IDF)

- Create a minimal ESP-IDF project structure in `firmware/`:
  - Top-level `CMakeLists.txt` with project name `esp32-ble-hid-typer`.
  - `main/CMakeLists.txt` with an empty `main.c` containing only `app_main()` with a log statement.
  - `main/idf_component.yml` with dependencies: `espressif/esp_tinyusb` and `espressif/led_strip`.
  - `sdkconfig.defaults` with base settings (target ESP32-S3, NimBLE stack, USB OTG enabled).
  - `sdkconfig.defaults.esp32s3` for chip-specific overrides.
  - `partitions.csv` with OTA support (factory + ota_0 + ota_1 + nvs + nvs_keys).
  - `ota_signing_pubkey.pem` — the public key for OTA signature verification (embedded in firmware, checked into repo).
- Verify that `idf.py set-target esp32s3 && idf.py build` succeeds in the DevContainer.

**Validation:** Build succeeds without errors. Binary is generated in `firmware/build/`.

### 1.4 Webapp build skeleton (Preact + Vite)

- Initialize a Preact + TypeScript project in `webapp/` with Vite as bundler.
- Configure `vite.config.ts` with:
  - `vite-plugin-pwa` for service worker and manifest.
  - `base` path set to the GitHub Pages subpath (`/<repo-name>/`).
- Create `public/manifest.json` with PWA metadata (name, colors, icons).
- Add placeholder icons (192x192 and 512x512).
- Create a minimal `index.html` and `src/index.tsx` that renders a "Hello World" Preact app.
- Verify that `npm ci && npm run build` succeeds and produces `dist/` output.

**Validation:** `npm run dev` starts a dev server. `npm run build` generates a `dist/` folder with `index.html`, service worker, and manifest.

### 1.5 GitHub Actions — Webapp deploy

- Create `.github/workflows/deploy-webapp.yml`:
  - Trigger: push to `main` with path filter `webapp/**`.
  - Steps: checkout → setup Node 20 → `npm ci` → `npm run build` → upload pages artifact → deploy to GitHub Pages.
  - Permissions: `contents: read`, `pages: write`, `id-token: write`.

**Validation:** YAML is syntactically correct. Workflow triggers on changes in `webapp/`.

### 1.6 GitHub Actions — Firmware build & sign for OTA

- Create `.github/workflows/build-firmware.yml`:
  - Trigger: push of tags `v*`.
  - Build job in `espressif/idf:v5.3` container.
  - Steps: checkout → `idf.py set-target esp32s3` → `idf.py build`.
  - Sign with `openssl dgst -sha256 -sign` using OTA signing key from `secrets.OTA_SIGNING_KEY`.
  - Rename binaries to `firmware-esp32s3.bin`, `firmware-esp32s3.sig`, `bootloader-esp32s3.bin`, `partition-table-esp32s3.bin`.
  - Upload as artifacts and create a GitHub Release with `softprops/action-gh-release`.

**Validation:** YAML is syntactically correct. Workflow references the correct secret (`OTA_SIGNING_KEY`). Release step uploads firmware binary, signature, bootloader, and partition table.

### 1.7 OTA signing key generation (local instructions)

- Add a section to `docs/OTA_SIGNING.md` describing how to generate the ECDSA P-256 key pair locally with `openssl`.
- Add `ota_signing_key.pem` (private key) to `.gitignore`.
- The public key `ota_signing_pubkey.pem` is checked into the repository and embedded in firmware.
- Document how the private key is stored as a GitHub Secret (`OTA_SIGNING_KEY`).

**Validation:** Documentation is complete. Public key is committed to the repo. Private key is gitignored and referenced as a GitHub Secret.

### Phase 1 — Final Checklist

- [ ] DevContainer builds and contains ESP-IDF + Node.js
- [ ] Firmware compiles for ESP32-S3 (empty app_main)
- [ ] Webapp builds and produces PWA-ready dist
- [ ] Both GitHub Actions workflows are present and syntactically correct
- [ ] OTA signing key instructions documented
- [ ] Repository structure matches the specification

---

## Phase 2 — Bluetooth (BLE) Functionality

**Goal:** A fully working BLE-only flow: provisioning → normal mode → text typing via USB HID. No WiFi, no WebSocket.

### 2.1 USB HID keyboard via TinyUSB

- Implement USB HID device initialization with TinyUSB via the ESP Component Registry (`espressif/esp_tinyusb`).
- Create a typing engine that converts UTF-8 text to HID keycodes (US layout).
- Implement rate limiting (configurable, default 10ms between keystrokes, max 1000 chars/min).
- Implement a text queue (max 8KB RAM) with abort capability.
- USB OTG pins: GPIO19 (D-), GPIO20 (D+).

**Validation:** ESP32-S3 is recognized as a USB HID keyboard. A hardcoded string is typed in a text editor on the target PC.

### 2.2 NeoPixel LED control

- Implement WS2812 NeoPixel control via the `espressif/led_strip` component and RMT peripheral.
- GPIO48 (standard on most ESP32-S3 DevKit boards).
- Implement all LED status codes from the specification:
  - Provisioning: orange slow blink (1s on/off)
  - Disconnected: off
  - BLE connected: solid blue
  - Typing: red flashing (500ms on/off)
  - Factory reset warning: yellow rapid flash (100ms)
  - Factory reset confirmed: solid red (1s)
  - Error: red rapid blink
  - OTA: purple pulsing
- Default brightness: 5%, configurable 1-100%.

**Validation:** Each LED status is visually distinguishable. Brightness is low but visible at 5%.

### 2.3 Encrypted NVS storage (partition-based)

- Initialize NVS with encryption using keys from the `nvs_keys` partition (no eFuse burning).
- Generate NVS encryption keys partition during build if not present.
- Create helper functions for securely storing and reading: PIN, WiFi credentials, certificate, audit log, configuration.
- Prepare NVS namespace layout (separate namespaces for credentials, config, certs, logs).

**Validation:** Data written to NVS is encrypted (not plaintext). Read/write cycle of test data succeeds. No eFuse operations are performed.

### 2.4 Provisioning mode — detection and BLE service

- Implement provisioning detection logic in `main.c`: if no PIN exists in NVS → provisioning mode.
- Start a BLE service with device name "ESP32-HID-SETUP" and Improv WiFi-compatible UUIDs.
- Implement the four GATT characteristics: Status, Error, RPC Command, RPC Result.
- USB HID is disabled in provisioning mode.
- LED blinks orange slowly.

**Validation:** After factory wipe, nRF Connect (or similar BLE scanner) shows the device as "ESP32-HID-SETUP" with the correct services. LED blinks orange.

### 2.5 Provisioning commands handler

- Implement the RPC command handler for:
  - `set_pin`: validate (6 digits, not 000000, not sequential/repetitive), store in encrypted NVS.
  - `set_wifi`: store SSID + password in encrypted NVS (WiFi connection is NOT made in this phase).
  - `complete`: verify PIN is set, send success response, reboot after 1 second.
- Send JSON responses via the RPC Result characteristic.

**Validation:** Via nRF Connect: write `set_pin` command → read success response. Write `complete` → device restarts in normal mode.

### 2.6 BLE GATT server — normal mode (NimBLE)

- Implement the NimBLE GATT server with the five characteristics from the specification:
  1. Text Input (Write) — UUID `6e400002`
  2. Status (Read, Notify) — UUID `6e400003`
  3. PIN Management (Write) — UUID `6e400004`
  4. WiFi Config (Write, Read) — UUID `6e400005` (stub in this phase)
  5. Certificate Fingerprint (Read) — UUID `6e400006` (stub in this phase)
- Device name in normal mode: "ESP32-HID-Typer".

**Validation:** nRF Connect shows all five characteristics with the correct UUIDs and permissions.

### 2.7 BLE LE Secure Connections

- Configure NimBLE for LE Secure Connections with passkey pairing.
- The passkey is the PIN set during provisioning.
- Reject unencrypted connections and legacy pairing.
- Keep only the most recent bonding (auto-cleanup of older bondings).
- Implement the bonding database in NVS.

**Validation:** When connecting via BLE, the OS prompts for a passkey. Only the correct PIN is accepted. A second device bonds successfully and the first bonding is removed.

### 2.8 PIN management and rate limiting

- Implement PIN storage and verification in encrypted NVS.
- Implement rate limiting: max 3 attempts per 60 seconds.
- Implement exponential backoff after failed attempts.
- Implement device lockout after 10 failed attempts (requires physical reset).
- Implement PIN change via BLE characteristic (requires old PIN).
- Implement "forced PIN change" on first BLE connection after provisioning.

**Validation:** After 3 incorrect PIN attempts, further authentication is blocked for 60s. After 10 attempts the device is locked. PIN change only works with the correct old PIN.

### 2.9 Text typing via BLE

- Connect the Text Input BLE characteristic to the typing engine.
- Received text is placed in the queue and typed character by character via USB HID.
- Status notifications send typing progress (current/total) via the Status characteristic.
- LED switches to red flashing during typing.

**Validation:** Write text to the Text Input characteristic via nRF Connect → text appears in a text editor on the target PC. Status notifications show progress.

### 2.10 BOOT button factory reset

- Implement GPIO0 monitoring in a FreeRTOS task.
- Timing: 0-2s no action, 2-10s yellow rapid flash (warning), 10s+ solid red → factory reset.
- Factory reset wipes: PIN, WiFi credentials, AP settings from NVS. Certificate is preserved.
- After reset: device restarts in provisioning mode.
- If button is released before 10s: reset cancelled, LED returns to normal status.

**Validation:** Hold BOOT button 5 seconds → yellow flashing. Release → normal. Hold 10+ seconds → reset → orange blinking (provisioning mode).

### 2.11 Serial console commands (optional)

- Implement a UART command handler (115200 baud, 8N1) with commands: `status`, `cert_fingerprint`, `factory_reset`, `full_reset`, `reboot`, `heap`, `wifi_scan`, `help`.
- `full_reset` wipes everything including certificate and bonding database.

**Validation:** Commands work via a serial terminal. `factory_reset` results in provisioning mode.

### 2.12 Audit logging

- Implement a 4KB ring buffer in RAM for log events in Syslog RFC5424 format.
- Logged events: auth attempts, factory resets, PIN changes. No sensitive data.
- Persist buffer to encrypted NVS at reboot.
- Load from NVS at boot.
- Expose a BLE characteristic or mechanism to retrieve logs (via an additional GATT characteristic or via the Status characteristic with a read-logs action).

**Validation:** After an auth attempt and reboot, logs are still available. Log format follows RFC5424.

### 2.13 PWA — Firmware flasher (Web Serial)

- Create the `FirmwareFlash.tsx` component with `esptool-js`.
- Use the Web Serial API to flash firmware to a connected ESP32-S3.
- No extra parameters needed — the flasher uses default offsets.
- Show progress and success/error messages.

**Validation:** Firmware can be flashed to an ESP32-S3 via USB through the PWA. Device boots after flashing.

### 2.14 PWA — Provisioning flow

- Create `ProvisioningScreen.tsx`:
  - Scan for BLE device with name "ESP32-HID-SETUP".
  - Detect provisioning status via the Status characteristic.
  - Show setup UI: PIN input (2x, with validation), optional WiFi SSID + password.
  - PIN validation: exactly 6 digits, not 000000, not sequential (123456, 654321), not repetitive (111111, etc.), confirmation must match.
  - Send `set_pin`, optionally `set_wifi`, then `complete` via RPC Command characteristic.
  - Show success message with instructions to reconnect.

**Validation:** Full provisioning flow via the PWA: device in provisioning mode → set PIN → complete → device restarts in normal mode.

### 2.15 PWA — BLE connection (normal mode)

- Create `BleConnect.tsx`:
  - Connect via Web Bluetooth API to device "ESP32-HID-Typer".
  - OS pairing prompt for passkey (the PIN).
  - Read Status characteristic to check `pin_set` status.
  - If `pin_set === false` or if this is the first connection after provisioning: redirect to PIN change screen.

- Create `PinSetup.tsx`:
  - Mandatory PIN change (old PIN + new PIN 2x).
  - Blocks all other functionality until PIN is changed.

**Validation:** BLE connection from the PWA, including pairing with PIN. Forced PIN change works and blocks the rest of the UI.

### 2.16 PWA — Text sending

- Create `TextSender.tsx`:
  - Textarea for text input.
  - "Send" button that writes text to the Text Input BLE characteristic.
  - Progress bar based on Status notifications.
  - "Abort" button to clear the queue.

- Create `ClipboardPaste.tsx`:
  - "Paste & Send" button that uses `navigator.clipboard.readText()`.

- Create `StatusBar.tsx`:
  - Connection status (BLE/disconnected).
  - Typing progress indicator.

**Validation:** Enter text in the PWA → send → text appears on the target PC via USB HID. Progress is visible. Abort stops typing.

### 2.17 PWA — Basic settings and audit log

- Create `Settings.tsx` with:
  - Typing speed setting (5ms - 100ms delay).
  - LED brightness setting (1-100%).
  - Change PIN.
  - SysRq opt-in toggle (UI only, SysRq functionality comes later).

- Create `AuditLog.tsx`:
  - Retrieve logs via BLE.
  - Display in a scrollable list.
  - Export option (copy to clipboard or download as text).

**Validation:** Settings are saved and applied. Audit logs are retrievable and readable.

### 2.18 PWA — App shell, routing and offline support

- Create `app.tsx` with routing (hash-based or history-based) to all screens:
  - Home / Connection screen
  - Provisioning
  - Text Sender
  - Settings
  - Firmware Flash
  - Audit Log
- `ConnectionScreen.tsx` automatically detects whether the device is in provisioning or normal mode.
- Service worker via `vite-plugin-pwa` for full offline support.

**Validation:** PWA is installable. All routes work. App works offline after first visit (except BLE connections of course).

### Phase 2 — Final Checklist

- [ ] Provisioning flow works end-to-end (PWA → BLE → ESP32)
- [ ] BLE pairing with PIN works
- [ ] Forced PIN change after first connection works
- [ ] Text typing via BLE → USB HID works
- [ ] LED statuses are correct
- [ ] Rate limiting and lockout work
- [ ] Factory reset via BOOT button works
- [ ] Audit logging works and persists
- [ ] PWA is installable and works offline
- [ ] Firmware flashing via Web Serial in the PWA works

---

## Phase 3 — Future option: WebSocket (WiFi) Functionality

**Goal:** Add WiFi connectivity, certificate management, WSS transport, OTA updates, SysRq, and all remaining features on top of the working BLE base.

### 3.1 WiFi manager

- Implement WiFi AP+STA coexistence via ESP-IDF WiFi driver.
- Implement network scan, connect, disconnect, saved networks management.
- Store WiFi credentials in encrypted NVS.
- Connect the WiFi Config BLE characteristic (UUID `6e400005`) to the WiFi manager:
  - Actions: `scan`, `connect`, `disconnect`, `list`, `forget`.
  - JSON request/response format.
- Implement LED status: solid white when WiFi is connected.

**Validation:** Via nRF Connect: send WiFi scan command → receive list of networks. Send connect → ESP32 connects to WiFi. LED turns white.

### 3.2 ECDSA certificate generation and storage

- Implement ECDSA P-256 certificate generation at first boot (via mbedTLS).
- Store the certificate and private key in encrypted NVS.
- Calculate and cache the SHA256 fingerprint.
- Print the fingerprint to the serial console at boot.
- Connect the Certificate Fingerprint BLE characteristic (UUID `6e400006`): return the 64-character hex SHA256 string.

**Validation:** Certificate is generated once. Fingerprint is consistent between BLE reading and serial output. After reboot the same certificate is available.

### 3.3 HTTP server for certificate download

- Start a plain HTTP server on port 80 that ONLY serves the certificate:
  - `GET /cert.pem` → PEM format certificate.
  - `GET /cert.crt` → DER format certificate.
- No other endpoints. No authentication (the certificate is public; fingerprint verification provides the security).

**Validation:** `curl http://<esp32-ip>/cert.pem` returns a valid PEM certificate. The SHA256 of the downloaded certificate matches the BLE fingerprint.

### 3.4 HTTPS server with WSS

- Start an HTTPS server on port 8443 with the generated ECDSA certificate.
- Implement a WebSocket endpoint at `/ws`.
- Authentication: `Authorization: PIN <6-digit-pin>` header on connection.
- Implement rate limiting on PIN attempts (same rules as BLE).
- Implement idle timeout: 5 minutes of inactivity → disconnect.
- Implement the full WebSocket message protocol:
  - Client → ESP32: `text`, `wifi_scan`, `wifi_connect`, `ota_update`, `sysrq`, `abort`, `get_logs`, `ping`.
  - ESP32 → Client: `status`, `wifi_scan_result`, `typing_progress`, `error`, `logs`, `pong`.
- Connect text messages to the typing engine.
- LED status: solid yellow when WSS connection is active.

**Validation:** After certificate import in OS trust store: `wss://<ip>:8443/ws` connection succeeds. Sending text via WSS → typed via USB HID.

### 3.5 Connectivity manager (BLE + WiFi transport model)

- Implement the "single active control transport" model from the specification:
  - Default: BLE enabled. When WSS session becomes active → BLE automatically disabled. When WSS disconnects → BLE re-enabled.
- Implement the configurable options (stored in encrypted NVS):
  - "Disable BLE when WiFi connected" (default: on).
  - "Permanently disable BLE".
  - "Enable BLE when WiFi not connected" (default: on).
- The `update_ble_state()` logic is called on every connection status change.

**Validation:** Connect via WSS → BLE is disabled (no longer visible to BLE scanners). Disconnect WSS → BLE visible again.

### 3.6 PWA — Certificate setup flow

- Create `CertificateSetup.tsx`:
  - Read the fingerprint via BLE.
  - Download the certificate via HTTP (`http://<ip>/cert.pem`).
  - Calculate the SHA256 of the downloaded certificate.
  - Compare with the BLE fingerprint.
  - On match: green checkmark + import instructions per OS (macOS Keychain, Windows certmgr, Linux ca-certificates).
  - On mismatch: red warning, abort, suggest factory reset.

**Validation:** Fingerprint verification succeeds with an authentic certificate. Deliberate mismatch (replace cert) is detected and rejected.

### 3.7 PWA — Network connect and WSS transport

- Create `NetworkConnect.tsx`:
  - IP address input.
  - PIN input.
  - "Connect" button that connects via `wss://<ip>:8443/ws` with Authorization header.
  - Connection status and error messages.

- Update `TextSender.tsx` and `StatusBar.tsx` to support both BLE and WSS as transport.
- Create `websocket.ts` utility with: connect, send, receive, reconnect logic, heartbeat (ping/pong).

**Validation:** Connection via WSS after certificate import. Sending text via WSS works identically to BLE.

### 3.8 PWA — WiFi configuration UI

- Create `WifiConfig.tsx`:
  - Show available networks (via BLE WiFi Config characteristic or via WSS).
  - Connect to selected network (SSID + password).
  - Show saved networks with option to forget.
  - Show current connection status and IP address.

**Validation:** WiFi scan shows available networks. Connecting to a network works. IP address is displayed.

### 3.9 OTA firmware updates

- Implement the firmware side of OTA with `esp_https_ota`:
  - Download firmware and detached signature from specified URL (GitHub Release).
  - Verify ECDSA P-256 signature at application level using the public key embedded in the firmware.
  - Flash to ota_1 partition only if signature is valid.
  - Set boot partition and restart.
  - Rollback on crash (automatic via ESP-IDF OTA rollback).
- No Secure Boot — verification is application-level only. USB flashing is always allowed.
- LED status: purple pulsing during OTA.
- Log OTA events in audit log.

- Create `OtaUpdate.tsx` in the PWA:
  - Check for new firmware version (GitHub Releases API).
  - Show current vs. available version.
  - "Update" button that starts OTA via WSS (`ota_update` message).
  - Progress indicator.

**Validation:** OTA update of correctly signed firmware succeeds. Firmware with invalid signature is rejected. Rollback works with intentionally corrupt firmware. USB flashing of unsigned firmware still works.

### 3.10 SysRq magic keys

- Implement the SysRq HID sequence in firmware: Alt + PrintScreen + key.
- Support all keys from the specification (h, b, c, d, e, f, i, k, m, n, o, p, q, r, s, t, u, v, w, z).
- Create the GATT characteristic (UUID `6e400007`) and WSS message (`type: sysrq`).
- Log every SysRq usage in the audit log.

- Create `SysRqPanel.tsx` in the PWA:
  - Hidden behind opt-in toggle (stored in localStorage).
  - Warning dialog when enabling.
  - Per action: confirmation dialog with description, "I understand" checkbox, 10-second cooldown.
  - "Safe Reboot (REISUB)" button that sends the full sequence with 2s delay between each step.

**Validation:** SysRq+h on a Linux target shows help in dmesg. REISUB sequence safely reboots a Linux system. Everything is logged.

### 3.11 PWA — Security information and serial monitor

- Create `SecurityWarning.tsx`:
  - Show security information about the device (no Secure Boot, partition-based NVS encryption).
  - Explain that OTA updates are signed but USB flashing is unrestricted.
  - Link to `docs/SECURITY.md`.

- Create `SerialMonitor.tsx` (optional):
  - Serial console via Web Serial API.
  - 115200 baud.
  - Show output, allow command input.

**Validation:** Warning appears after flash. Serial monitor shows ESP32 boot log and accepts commands.

### 3.12 PWA — Guide, Improv WiFi, and polish

- Create `Guide.tsx`:
  - Step-by-step setup instructions.
  - Explanation per feature.
  - Security warnings and best practices.

- Implement full Improv WiFi protocol compliance in firmware (optional, for compatibility with generic Improv tools).

- Finalize `ConnectionScreen.tsx`:
  - Automatic detection of provisioning vs. normal mode.
  - Choice between BLE and Network connection.
  - Status overview.

- Apply theme and styling for a professional look.

**Validation:** Guide is complete and understandable. ConnectionScreen detects the correct mode. Improv WiFi tools can set WiFi credentials.

### 3.13 Performance monitoring and debugging

- Implement heap/stack monitoring behind a `DEBUG_PERF_MONITOR` flag.
- Log heap usage every 10 seconds to serial.
- Check for memory leaks after extended use.

**Validation:** When compiled with DEBUG flag: heap info appears in serial output. No significant memory leaks after 1 hour of use.

### 3.14 Documentation

- Write `docs/SECURITY.md`: full security architecture documentation (including rationale for no Secure Boot / no eFuse).
- Write `docs/OTA_SIGNING.md`: OTA signing key generation and firmware signing instructions.
- Write `docs/HARDWARE.md`: required hardware and board compatibility.
- Write `docs/THREAT_MODEL.md`: threat model with scenarios and mitigations (including physical access risks without Secure Boot).
- Update `README.md` with project overview, quick start, and links to docs.

**Validation:** All four documents are complete, accurate, and consistent with the implementation.

### Phase 3 — Final Checklist

- [ ] WiFi connection and management work via BLE
- [ ] ECDSA certificate is generated and consistent
- [ ] HTTP certificate download and fingerprint verification work
- [ ] WSS connection works after certificate import
- [ ] Text typing via WSS → USB HID works
- [ ] Connectivity manager correctly switches between BLE and WiFi
- [ ] OTA updates work (correctly signed accepted, invalid signature rejected, rollback works, USB flash still unrestricted)
- [ ] SysRq works with all safety controls
- [ ] All PWA screens are complete and functional
- [ ] Audit logging logs all events correctly
- [ ] Performance monitoring shows no memory leaks
- [ ] All documentation is complete
- [ ] End-to-end flow works: flash → provisioning → BLE use → certificate setup → WSS use → OTA update → factory reset → re-provisioning