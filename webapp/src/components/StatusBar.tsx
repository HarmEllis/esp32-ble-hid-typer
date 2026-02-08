import { useEffect, useState } from "preact/hooks";
import * as ble from "../utils/ble";
import type { DeviceStatus } from "../types/protocol";

export function StatusBar() {
  const [status, setStatus] = useState<DeviceStatus | null>(null);
  const [connected, setConnected] = useState(ble.isConnected());

  useEffect(() => {
    const conn = ble.getConnection();
    if (!conn || conn.mode !== "normal") return;

    ble.onStatusChange((value) => {
      try {
        setStatus(JSON.parse(value));
      } catch {
        /* ignore parse errors */
      }
    });

    ble.onDisconnect(() => {
      setConnected(false);
      setStatus(null);
    });

    /* Initial read */
    ble.readStatus().then((value) => {
      try {
        setStatus(JSON.parse(value));
        setConnected(true);
      } catch {
        /* ignore */
      }
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
