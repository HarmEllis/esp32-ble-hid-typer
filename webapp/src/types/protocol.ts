/* BLE UUIDs and protocol types */

/* Provisioning mode (Improv WiFi compatible) */
export const PROVISIONING_DEVICE_NAME = "ESP32-HID-SETUP";
export const PROVISIONING_SERVICE_UUID = "00467768-6228-2272-4663-277478268000";
export const PROVISIONING_STATUS_UUID = "00467768-6228-2272-4663-277478268001";
export const PROVISIONING_ERROR_UUID = "00467768-6228-2272-4663-277478268002";
export const PROVISIONING_RPC_COMMAND_UUID =
  "00467768-6228-2272-4663-277478268003";
export const PROVISIONING_RPC_RESULT_UUID =
  "00467768-6228-2272-4663-277478268004";

/* Normal mode */
export const NORMAL_DEVICE_NAME = "ESP32-HID-Typer";
export const NORMAL_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
export const TEXT_INPUT_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
export const STATUS_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
export const PIN_MANAGEMENT_UUID = "6e400004-b5a3-f393-e0a9-e50e24dcca9e";
export const WIFI_CONFIG_UUID = "6e400005-b5a3-f393-e0a9-e50e24dcca9e";
export const CERT_FINGERPRINT_UUID = "6e400006-b5a3-f393-e0a9-e50e24dcca9e";

/* Provisioning status values */
export enum ProvisioningStatus {
  Ready = 0,
  Provisioning = 1,
  Provisioned = 2,
}

/* Provisioning error values */
export enum ProvisioningError {
  None = 0,
  InvalidPin = 1,
  UnableToConnect = 2,
  Unknown = 3,
}

/* Provisioning RPC commands */
export interface SetPinCommand {
  command: "set_pin";
  pin: string;
}

export interface SetWifiCommand {
  command: "set_wifi";
  ssid: string;
  password: string;
}

export interface CompleteCommand {
  command: "complete";
}

export type ProvisioningCommand =
  | SetPinCommand
  | SetWifiCommand
  | CompleteCommand;

export interface ProvisioningResponse {
  success: boolean;
  message: string;
}

/* Normal mode types */
export interface DeviceStatus {
  connected: boolean;
  typing: boolean;
  queue: number;
}

export interface PinSetAction {
  action: "set";
  old: string;
  new: string;
}

export interface PinVerifyAction {
  action: "verify";
  pin: string;
}

export interface SetConfigAction {
  action: "set_config";
  key: string;
  value: string;
}

export interface GetLogsAction {
  action: "get_logs";
}

export interface AbortAction {
  action: "abort";
}

export type PinManagementAction =
  | PinSetAction
  | PinVerifyAction
  | SetConfigAction
  | GetLogsAction
  | AbortAction;
