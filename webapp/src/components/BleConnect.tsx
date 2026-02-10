import { useState } from "preact/hooks";
import { RoutableProps } from "preact-router";
import * as ble from "../utils/ble";
import { nav } from "../utils/nav";
import type { DeviceStatus } from "../types/protocol";
import { PageHeader } from "./PageHeader";

export function BleConnect(_props: RoutableProps) {
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);
  const [connected, setConnected] = useState(false);
  const [pin, setPin] = useState("");
  const [retryDelayMs, setRetryDelayMs] = useState(0);
  const [lockedOut, setLockedOut] = useState(false);

  const updateAuthStatus = (status: DeviceStatus) => {
    setRetryDelayMs(status.retry_delay_ms ?? 0);
    setLockedOut(Boolean(status.locked_out));
  };

  const getAuthError = (status: DeviceStatus): string => {
    if (status.locked_out || status.auth_error === "locked_out") {
      return "Device is locked out. Use serial reset to clear lockout.";
    }
    if (status.auth_error === "rate_limited" || status.retry_delay_ms > 0) {
      const seconds = Math.ceil(status.retry_delay_ms / 1000);
      return `Too many attempts. Try again in ${seconds}s.`;
    }
    if (status.auth_error === "invalid_pin") {
      return "Incorrect PIN";
    }
    return "PIN verification failed";
  };

  const handleConnect = async () => {
    if (busy || connected) return;
    setError("");
    setPin("");
    setRetryDelayMs(0);
    setLockedOut(false);
    setBusy(true);
    try {
      await ble.scanAndConnect("normal");
      setConnected(true);
      setBusy(false);

      ble.onDisconnect(() => {
        setConnected(false);
        setPin("");
        setRetryDelayMs(0);
        setLockedOut(false);
      });
      setError("");
    } catch (e) {
      setConnected(false);
      setError(e instanceof Error ? e.message : "Connection failed");
      setBusy(false);
    }
  };

  const handleUnlock = async () => {
    if (busy || !connected) return;
    setError("");
    if (!/^\d{6}$/.test(pin)) {
      setError("Enter a 6-digit PIN");
      return;
    }

    setBusy(true);
    try {
      const status = await ble.authenticate(pin);
      updateAuthStatus(status);
      if (status.authenticated) {
        nav("/send");
        return;
      }
      setError(getAuthError(status));
    } catch (e) {
      const message = e instanceof Error ? e.message : "PIN verification failed";
      if (/GATT operation already in progress/i.test(message)) {
        setError("Bluetooth still initializing. Wait a second and try Unlock again.");
      } else {
        setError(message);
      }
    } finally {
      setBusy(false);
    }
  };

  const handleDisconnect = async () => {
    await ble.disconnect();
    setConnected(false);
    setPin("");
    setRetryDelayMs(0);
    setLockedOut(false);
  };

  return (
    <div style={{ padding: "2rem", maxWidth: "500px", margin: "0 auto" }}>
      <PageHeader title="Connect to Device" />
      <p style={{ color: "#94a3b8", marginBottom: "1rem" }}>
        Connect to a provisioned device (blue LED when connected), then enter
        the device PIN here to unlock commands.
      </p>

      {!connected && (
        <button
          onClick={handleConnect}
          disabled={busy}
          style={{
            padding: "0.75rem 1.5rem",
            background: "#3b82f6",
            color: "white",
            border: "none",
            borderRadius: "8px",
            fontSize: "1rem",
            cursor: busy ? "not-allowed" : "pointer",
            opacity: busy ? 0.5 : 1,
          }}
        >
          {busy ? "Connecting..." : "Scan & Connect"}
        </button>
      )}

      {connected && (
        <div>
          <p style={{ color: "#4ade80", marginBottom: "0.75rem" }}>Connected</p>
          <input
            type="password"
            inputMode="numeric"
            maxLength={6}
            value={pin}
            onInput={(e) => setPin((e.target as HTMLInputElement).value)}
            onKeyDown={(e) => {
              if (e.key === "Enter") {
                handleUnlock();
              }
            }}
            placeholder="Enter 6-digit PIN"
            style={{
              width: "100%",
              padding: "0.75rem",
              background: "#1e293b",
              border: "1px solid #334155",
              borderRadius: "8px",
              color: "white",
              fontSize: "1rem",
              letterSpacing: "0.2rem",
              boxSizing: "border-box",
            }}
          />
          <div
            style={{
              display: "flex",
              gap: "0.5rem",
              marginTop: "0.75rem",
              flexWrap: "wrap",
            }}
          >
            <button
              onClick={handleUnlock}
              disabled={busy || lockedOut || retryDelayMs > 0}
              style={{
                padding: "0.6rem 1.2rem",
                background: "#22c55e",
                color: "white",
                border: "none",
                borderRadius: "8px",
                cursor: busy || lockedOut || retryDelayMs > 0 ? "not-allowed" : "pointer",
                opacity: busy || lockedOut || retryDelayMs > 0 ? 0.5 : 1,
              }}
            >
              {busy ? "Unlocking..." : "Unlock"}
            </button>
            <button
              onClick={handleDisconnect}
              disabled={busy}
              style={{
                padding: "0.6rem 1.2rem",
                background: "transparent",
                color: "#94a3b8",
                border: "1px solid #334155",
                borderRadius: "8px",
                cursor: busy ? "not-allowed" : "pointer",
              }}
            >
              Disconnect
            </button>
          </div>
          {retryDelayMs > 0 && !lockedOut && (
            <p style={{ color: "#f97316", marginTop: "0.75rem" }}>
              Retry available in {Math.ceil(retryDelayMs / 1000)}s
            </p>
          )}
          {lockedOut && (
            <p style={{ color: "#ef4444", marginTop: "0.75rem" }}>
              Device is locked out due to repeated failed attempts.
            </p>
          )}
        </div>
      )}

      {error && <p style={{ color: "#ef4444", marginTop: "1rem" }}>{error}</p>}
    </div>
  );
}
