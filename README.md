# ESP32 BLE HID Typer

An ESP32-S3 that acts as a USB HID keyboard. A Preact PWA connects to it via Bluetooth Low Energy and sends text that the ESP32 types out over USB. Use cases include pasting passwords, configs, or any text into machines where you can't easily paste.

## Quick Start

### Prerequisites

- VS Code with the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension
- Docker
- A Chromium-based browser (Chrome, Edge) for Web Serial flashing

### DevContainer Setup

1. Clone the repo and open it in VS Code
2. When prompted, click **Reopen in Container** (or run `Dev Containers: Reopen in Container` from the command palette)
3. Wait for the container to build — this installs ESP-IDF v5.3 and Node.js

The devcontainer is for **building only**. Flashing and serial monitoring happen via the browser (Web Serial API), so no USB passthrough is needed.

#### Persisting Claude Code config

The devcontainer bind-mounts `~/.claude/` and `~/.claude.json` from your host so that Claude Code sessions, memory, and auth persist across container rebuilds.

If you haven't used Claude Code on your host before, create the file first to prevent Docker from mounting it as a directory:

```bash
touch ~/.claude.json
```

### Build Commands

```bash
# Build firmware
cd firmware
idf.py set-target esp32s3
idf.py build

# Build webapp (dev server)
cd webapp && npm run dev

# Build webapp (production)
cd webapp && npm run build
```

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

- **BLE**: Web Bluetooth API, LE Secure Connections with PIN pairing

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
