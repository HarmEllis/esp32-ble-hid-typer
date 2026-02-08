# OTA Firmware Signing

This project uses ECDSA P-256 signatures for OTA firmware updates. The signature is verified at the application level (not Secure Boot). USB flashing is always allowed with unsigned firmware.

## Generate Key Pair (one-time)

```bash
# Generate private key
openssl ecparam -genkey -name prime256v1 -out ota_signing_key.pem

# Extract public key
openssl ec -in ota_signing_key.pem -pubout -out firmware/ota_signing_pubkey.pem
```

- **Private key** (`ota_signing_key.pem`): Keep secret. Add to GitHub Secrets as `OTA_SIGNING_KEY`.
- **Public key** (`firmware/ota_signing_pubkey.pem`): Committed to the repository, embedded in the firmware binary.

## Sign Firmware Manually

```bash
openssl dgst -sha256 -sign ota_signing_key.pem \
    -out firmware/build/firmware.sig \
    firmware/build/esp32-ble-hid-typer.bin
```

## CI/CD

The `build-firmware.yml` GitHub Action signs firmware automatically using the `OTA_SIGNING_KEY` secret. Add the private key to your repository's GitHub Secrets:

1. Go to **Settings > Secrets and variables > Actions**
2. Click **New repository secret**
3. Name: `OTA_SIGNING_KEY`
4. Value: paste the full contents of `ota_signing_key.pem`

## Verify Signature

```bash
openssl dgst -sha256 -verify firmware/ota_signing_pubkey.pem \
    -signature firmware.sig \
    firmware.bin
```

Expected output: `Verified OK`
