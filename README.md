# ESP32 BLE HID Typer

Live app: [https://harmellis.github.io/esp32-ble-hid-typer/](https://harmellis.github.io/esp32-ble-hid-typer/)

An ESP32-S3 that acts as a USB HID keyboard. A Preact PWA connects to it via Bluetooth Low Energy and sends text that the ESP32 types out over USB. Use cases include pasting passwords, configs, or any text into machines where you can't easily paste.

## Get Started (Hosted Web App)

### Requirements

- An ESP32-S3 board with USB OTG support (GPIO19/GPIO20)
- A Chromium-based browser with Web Bluetooth and Web Serial support (Chrome, Edge, Opera)
- A USB data cable
- A target machine where the ESP32 will act as a USB keyboard

### First-Time Setup

1. Open the live app URL in your browser:
   [https://harmellis.github.io/esp32-ble-hid-typer/](https://harmellis.github.io/esp32-ble-hid-typer/)
2. Click **Flash Firmware**.
3. Put the ESP32-S3 into download mode (hold `BOOT`, tap `RESET`, then release `BOOT`).
4. In the app, click **Connect to ESP32** and select the serial port.
5. In **Firmware Source**, choose either:
   - **GitHub Release** (recommended): pick a release tag and click **Flash Firmware**
   - **Local Files**: upload bootloader, partition table, and firmware `.bin` files, then click **Flash Firmware**
6. Wait for the app to finish flashing and reset the board.
7. From the home screen, click **Set Up New Device**.
8. Click **Scan for Device** and choose the BLE device named `ESP32-HID-SETUP`.
9. Set and confirm a 6-digit PIN, then click **Complete Setup**.
10. After the board reboots, click **Connect to Device**.
11. Click **Scan & Connect** and choose the BLE device named `ESP32-HID-Typer`.
12. Enter your 6-digit PIN and click **Unlock**.
13. Connect the ESP32 USB OTG interface to the target machine, then go to **Send Text**, type or paste text, and click **Send**.

### Daily Use

1. Open the live app URL.
2. Click **Connect to Device**.
3. Click **Scan & Connect**, select `ESP32-HID-Typer`, and unlock with your PIN.
4. Use **Send Text** to type on the target machine through the ESP32 keyboard.

### Troubleshooting

- If Bluetooth scan shows no device, confirm the board is powered and nearby.
- If setup scan fails, ensure the device is in provisioning mode (`ESP32-HID-SETUP`, orange blinking LED).
- If the app reports keyboard not ready, connect the ESP32 USB OTG interface to the target machine first.
- If PIN attempts are rate-limited, wait for the countdown before retrying.

## Quick Start (Development)

### Prerequisites

- VS Code with the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension
- Docker
- A Chromium-based browser (Chrome, Edge) for BLE/Web Serial testing

### DevContainer Setup

1. Clone the repo and open it in VS Code.
2. Click **Reopen in Container** (or run `Dev Containers: Reopen in Container`).
3. Wait for the container to build (ESP-IDF v5.3 + Node.js).

The devcontainer is for **building only**. Flashing and serial monitoring happen via browser APIs, so USB passthrough is not required.

#### Persisting Claude Code config

The devcontainer bind-mounts `~/.claude/` and `~/.claude.json` from your host so Claude Code sessions, memory, and auth persist across rebuilds.

If you have not used Claude Code on your host before, create the file first to prevent Docker from mounting it as a directory:

```bash
touch ~/.claude.json
```

### Development Commands

```bash
# Webapp dependencies
cd webapp
npm ci

# Webapp dev server
npm run dev

# Webapp production build
npm run build

# Firmware build
cd ../firmware
idf.py set-target esp32s3
idf.py build
```

### Signing & Flashing Firmware Locally

#### 1. Generate an OTA signing key pair (one-time)

```bash
# Generate ECDSA P-256 private key
openssl ecparam -genkey -name prime256v1 -out ota_signing_key.pem

# Extract public key (committed to the repo, embedded in firmware)
openssl ec -in ota_signing_key.pem -pubout -out firmware/ota_signing_pubkey.pem
```

Keep `ota_signing_key.pem` secret — it is gitignored. See [docs/OTA_SIGNING.md](docs/OTA_SIGNING.md) for CI/CD setup with GitHub Secrets.

#### 2. Build the firmware

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
```

The binary is written to `firmware/build/esp32-ble-hid-typer.bin`.

#### 3. Sign the firmware for OTA

```bash
openssl dgst -sha256 -sign ota_signing_key.pem \
    -out build/firmware.sig \
    build/esp32-ble-hid-typer.bin
```

To verify the signature:

```bash
openssl dgst -sha256 -verify ota_signing_pubkey.pem \
    -signature build/firmware.sig \
    build/esp32-ble-hid-typer.bin
# Expected output: Verified OK
```

#### 4. Flash via USB

**Option A — PWA Web Serial flasher (recommended)**

Open [https://harmellis.github.io/esp32-ble-hid-typer/](https://harmellis.github.io/esp32-ble-hid-typer/) in a Chromium browser, navigate to the **Flash Firmware** page, connect the ESP32-S3 via USB, and follow the on-screen instructions. No extra tools needed.

**Option B — esptool.py from the DevContainer**

If you have a USB-UART bridge connected and USB passthrough configured:

```bash
cd firmware
idf.py -p /dev/ttyUSB0 flash
```

> **Note:** USB flashing always accepts unsigned firmware. OTA signing is only enforced for over-the-air updates.

### Serial Console (Optional)

The ESP32-S3 shares its internal USB PHY between USB-OTG (used for HID keyboard) and USB-Serial-JTAG. Once TinyUSB initializes, USB-Serial-JTAG is no longer available. To access the serial console you need a **separate USB-UART bridge** (e.g. CP2102, CH340) wired to dedicated UART TX/RX pins on the board.

Connect the USB-UART bridge and open the console at **115200 baud, 8N1**:

```bash
# Linux
idf.py -p /dev/ttyUSB0 monitor

# macOS
idf.py -p /dev/cu.usbserial-* monitor

# Or any serial terminal (minicom, screen, PuTTY, etc.)
screen /dev/ttyUSB0 115200
```

Type `help` to see available commands:

| Command | Description |
|---------|-------------|
| `status` | Show device status, heap usage, PIN state |
| `heap` | Show detailed heap usage |
| `factory_reset` | Wipe PIN and WiFi credentials, reboot to provisioning mode |
| `full_reset` | Wipe everything (including certificates), reboot to provisioning mode |
| `reboot` | Reboot the device |
| `help` | List available commands |

> The serial console is **not required** for normal use. All essential operations (flashing, provisioning, factory reset) are available through the PWA and the BOOT button.

## Project Structure

```
├── .devcontainer/     # VS Code DevContainer (ESP-IDF + Node.js)
├── firmware/          # ESP-IDF firmware for ESP32-S3
├── webapp/            # Preact PWA (GitHub Pages)
├── docs/              # Security docs, threat model, OTA signing
├── CLAUDE.md          # Full architecture and protocol reference
├── FEATURES.md        # Feature overview
└── PLAN.md            # Implementation plan and phases
```

## Architecture

The PWA is hosted on GitHub Pages (free HTTPS). It communicates with the ESP32 exclusively over BLE.

```
Preact PWA ── BLE GATT ──> ESP32-S3 ── USB HID ──> Target PC
```

- **BLE**: Web Bluetooth API with app-layer PIN unlock

See [CLAUDE.md](CLAUDE.md) for the full architecture, protocols, and security model.

## Hardware

| Chip | USB OTG Pins | BLE | WiFi | Status |
|------|-------------|-----|------|--------|
| ESP32-S3 | GPIO19 (D-), GPIO20 (D+) | Yes | Yes | **Supported** |
| ESP32 | Unsupported | Yes | Yes | Unsupported |
| ESP32-2 | Per datasheet | No | Yes | Unsupported |
| ESP32-P4 | Per datasheet | Yes | Yes | Unsupported |

Any ESP32-S3 board with USB OTG support, a built-in WS2812 NeoPixel LED (GPIO48), and a BOOT button (GPIO0).

## License

MIT
