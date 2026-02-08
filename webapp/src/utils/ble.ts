/* Web Bluetooth API wrapper */

import {
  PROVISIONING_DEVICE_NAME,
  PROVISIONING_SERVICE_UUID,
  PROVISIONING_RPC_COMMAND_UUID,
  PROVISIONING_RPC_RESULT_UUID,
  PROVISIONING_STATUS_UUID,
  PROVISIONING_ERROR_UUID,
  NORMAL_DEVICE_NAME,
  NORMAL_SERVICE_UUID,
  TEXT_INPUT_UUID,
  STATUS_UUID,
  PIN_MANAGEMENT_UUID,
  CERT_FINGERPRINT_UUID,
} from "../types/protocol";

export type BleMode = "provisioning" | "normal";

export interface BleConnection {
  device: BluetoothDevice;
  server: BluetoothRemoteGATTServer;
  service: BluetoothRemoteGATTService;
  mode: BleMode;
}

const encoder = new TextEncoder();
const decoder = new TextDecoder();

let currentConnection: BleConnection | null = null;

function getServiceUuid(mode: BleMode): string {
  return mode === "provisioning"
    ? PROVISIONING_SERVICE_UUID
    : NORMAL_SERVICE_UUID;
}

function getDeviceName(mode: BleMode): string {
  return mode === "provisioning"
    ? PROVISIONING_DEVICE_NAME
    : NORMAL_DEVICE_NAME;
}

export async function scanAndConnect(mode: BleMode): Promise<BleConnection> {
  const serviceUuid = getServiceUuid(mode);

  const device = await navigator.bluetooth.requestDevice({
    filters: [{ namePrefix: getDeviceName(mode) }],
    optionalServices: [serviceUuid],
  });

  const server = await device.gatt!.connect();
  const service = await server.getPrimaryService(serviceUuid);

  currentConnection = { device, server, service, mode };

  device.addEventListener("gattserverdisconnected", () => {
    currentConnection = null;
  });

  return currentConnection;
}

export function getConnection(): BleConnection | null {
  if (currentConnection && !currentConnection.server.connected) {
    currentConnection = null;
  }
  return currentConnection;
}

export function isConnected(): boolean {
  return getConnection() !== null;
}

export async function disconnect(): Promise<void> {
  if (currentConnection?.server.connected) {
    currentConnection.server.disconnect();
  }
  currentConnection = null;
}

export async function readCharacteristic(uuid: string): Promise<string> {
  const conn = getConnection();
  if (!conn) throw new Error("Not connected");
  const char = await conn.service.getCharacteristic(uuid);
  const value = await char.readValue();
  return decoder.decode(value);
}

export async function writeCharacteristic(
  uuid: string,
  data: string
): Promise<void> {
  const conn = getConnection();
  if (!conn) throw new Error("Not connected");
  const char = await conn.service.getCharacteristic(uuid);
  const encoded = encoder.encode(data);

  /* Chunk writes for data > 512 bytes */
  const MTU = 512;
  for (let offset = 0; offset < encoded.length; offset += MTU) {
    const chunk = encoded.slice(offset, offset + MTU);
    await char.writeValueWithResponse(chunk);
  }
}

export async function startNotifications(
  uuid: string,
  callback: (value: string) => void
): Promise<void> {
  const conn = getConnection();
  if (!conn) throw new Error("Not connected");
  const char = await conn.service.getCharacteristic(uuid);
  char.addEventListener("characteristicvaluechanged", (event) => {
    const target = event.target as BluetoothRemoteGATTCharacteristic;
    callback(decoder.decode(target.value!));
  });
  await char.startNotifications();
}

/* Provisioning helpers */

export async function sendProvisioningCommand(
  command: object
): Promise<string> {
  const conn = getConnection();
  if (!conn || conn.mode !== "provisioning")
    throw new Error("Not in provisioning mode");

  /* Set up result notification listener before writing command */
  const resultChar = await conn.service.getCharacteristic(
    PROVISIONING_RPC_RESULT_UUID
  );

  const resultPromise = new Promise<string>((resolve) => {
    const handler = (event: Event) => {
      const target = event.target as BluetoothRemoteGATTCharacteristic;
      const value = decoder.decode(target.value!);
      resultChar.removeEventListener("characteristicvaluechanged", handler);
      resolve(value);
    };
    resultChar.addEventListener("characteristicvaluechanged", handler);
  });

  await resultChar.startNotifications();
  await writeCharacteristic(PROVISIONING_RPC_COMMAND_UUID, JSON.stringify(command));

  return resultPromise;
}

export async function readProvisioningStatus(): Promise<number> {
  const data = await readCharacteristic(PROVISIONING_STATUS_UUID);
  return data.charCodeAt(0);
}

export async function readProvisioningError(): Promise<number> {
  const data = await readCharacteristic(PROVISIONING_ERROR_UUID);
  return data.charCodeAt(0);
}

/* Normal mode helpers */

export async function sendText(text: string): Promise<void> {
  await writeCharacteristic(TEXT_INPUT_UUID, text);
}

export async function readStatus(): Promise<string> {
  return readCharacteristic(STATUS_UUID);
}

export async function sendPinAction(action: object): Promise<void> {
  await writeCharacteristic(PIN_MANAGEMENT_UUID, JSON.stringify(action));
}

export async function readCertFingerprint(): Promise<string> {
  return readCharacteristic(CERT_FINGERPRINT_UUID);
}

export async function onStatusChange(
  callback: (value: string) => void
): Promise<void> {
  await startNotifications(STATUS_UUID, callback);
}

export function onDisconnect(callback: () => void): void {
  if (currentConnection?.device) {
    currentConnection.device.addEventListener(
      "gattserverdisconnected",
      callback
    );
  }
}
