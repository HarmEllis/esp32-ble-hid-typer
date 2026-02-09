import { useEffect, useState } from "preact/hooks";
import * as ble from "../utils/ble";
import type { DeviceStatus } from "../types/protocol";

export function StatusBar() {
  const [status, setStatus] = useState<DeviceStatus | null>(null);
  const [connected, setConnected] = useState(ble.isConnected());

  const applyUpdate = (update: Partial<DeviceStatus>) => {
    setStatus((prev) => {
      const base: DeviceStatus = prev ?? {
        connected: true,
        typing: false,
        queue: 0,
        authenticated: false,
        retry_delay_ms: 0,
        locked_out: false,
      };
      return { ...base, ...update };
    });
    setConnected(true);
  };

  useEffect(() => {
    const conn = ble.getConnection();
    if (!conn || conn.mode !== "normal") return;

    ble.onStatusChange((value) => {
      try {
        applyUpdate(JSON.parse(value) as Partial<DeviceStatus>);
      } catch {
        /* ignore parse errors */
      }
    }).catch(() => {
      /* Subscription may fail during security negotiation */
    });

    ble.onDisconnect(() => {
      setConnected(false);
      setStatus(null);
    });

    /* Initial read — may fail if security is still being established */
    ble.readStatusObject().then((value) => {
      try {
        applyUpdate(value);
      } catch {
        /* ignore parse errors */
      }
    }).catch(() => {
      /* Read may fail during security negotiation */
    });
  }, []);

  if (!connected) return null;

  return (
    <div
      style={{
        padding: "0.5rem 1rem",
        background: "#2a2a4a",
        borderRadius: "8px",
        marginBottom: "1rem",
        fontSize: "0.85rem",
      }}
    >
      <span style={{ color: "#4ade80" }}>Connected</span>
      {status && (
        <>
          <span style={{ margin: "0 0.5rem" }}>|</span>
          <span style={{ color: status.authenticated ? "#22c55e" : "#f59e0b" }}>
            {status.authenticated ? "Unlocked" : "Locked"}
          </span>
        </>
      )}
      {status && !status.authenticated && status.retry_delay_ms > 0 && (
        <>
          <span style={{ margin: "0 0.5rem" }}>|</span>
          <span style={{ color: "#f97316" }}>
            Retry in {Math.ceil(status.retry_delay_ms / 1000)}s
          </span>
        </>
      )}
      {status?.locked_out && (
        <>
          <span style={{ margin: "0 0.5rem" }}>|</span>
          <span style={{ color: "#ef4444" }}>Lockout active</span>
        </>
      )}
      {status?.typing && (
        <>
          <span style={{ margin: "0 0.5rem" }}>|</span>
          <span style={{ color: "#f97316" }}>Typing...</span>
          {status.queue > 0 && (
            <span style={{ color: "#94a3b8", marginLeft: "0.5rem" }}>
              ({status.queue} queued)
            </span>
          )}
        </>
      )}
    </div>
  );
}
