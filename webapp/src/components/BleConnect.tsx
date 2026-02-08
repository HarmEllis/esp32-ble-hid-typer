import { useState } from "preact/hooks";
import { RoutableProps } from "preact-router";
import * as ble from "../utils/ble";
import { nav } from "../utils/nav";

export function BleConnect(_props: RoutableProps) {
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);

  const handleConnect = async () => {
    setError("");
    setBusy(true);
    try {
      await ble.scanAndConnect("normal");
      nav("/send");
    } catch (e) {
      setError(e instanceof Error ? e.message : "Connection failed");
    } finally {
      setBusy(false);
    }
  };

  return (
    <div style={{ padding: "2rem", maxWidth: "500px", margin: "0 auto" }}>
      <h2>Connect to Device</h2>
      <p style={{ color: "#94a3b8", marginBottom: "1rem" }}>
        Connect to a provisioned device (blue LED when connected). Your OS
        will prompt for the passkey (the PIN you set during setup).
      </p>
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
      {error && <p style={{ color: "#ef4444", marginTop: "1rem" }}>{error}</p>}
    </div>
  );
}
