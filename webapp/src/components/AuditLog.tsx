import { useState } from "preact/hooks";
import { RoutableProps } from "preact-router";
import * as ble from "../utils/ble";
import { PageHeader } from "./PageHeader";

export function AuditLog(_props: RoutableProps) {
  const [logs, setLogs] = useState("");
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  const handleFetch = async () => {
    setError("");
    setLoading(true);
    try {
      await ble.sendPinAction({ action: "get_logs" });
      /* Read the response from status characteristic */
      const response = await ble.readStatus();
      setLogs(response);
    } catch (e) {
      setError(e instanceof Error ? e.message : "Failed to fetch logs");
    } finally {
      setLoading(false);
    }
  };

  const handleCopy = async () => {
    if (logs) {
      await navigator.clipboard.writeText(logs);
    }
  };

  return (
    <div style={{ padding: "2rem", maxWidth: "600px", margin: "0 auto" }}>
      <PageHeader title="Audit Log" />

      <div style={{ display: "flex", gap: "0.5rem", marginBottom: "1rem" }}>
        <button
          onClick={handleFetch}
          disabled={loading || !ble.isConnected()}
          style={{
            padding: "0.5rem 1rem",
            background: "#3b82f6",
            color: "white",
            border: "none",
            borderRadius: "6px",
            cursor: loading ? "not-allowed" : "pointer",
            opacity: loading ? 0.5 : 1,
          }}
        >
          {loading ? "Loading..." : "Fetch Logs"}
        </button>
        {logs && (
          <button
            onClick={handleCopy}
            style={{
              padding: "0.5rem 1rem",
              background: "#1e293b",
              color: "#94a3b8",
              border: "1px solid #334155",
              borderRadius: "6px",
              cursor: "pointer",
            }}
          >
            Copy to Clipboard
          </button>
        )}
      </div>

      {!ble.isConnected() && (
        <p style={{ color: "#f97316", fontSize: "0.85rem" }}>
          Connect to device first to fetch logs.
        </p>
      )}

      {error && <p style={{ color: "#ef4444" }}>{error}</p>}

      {logs && (
        <pre
          style={{
            background: "#0f172a",
            padding: "1rem",
            borderRadius: "8px",
            overflow: "auto",
            maxHeight: "400px",
            fontSize: "0.8rem",
            color: "#e2e8f0",
            whiteSpace: "pre-wrap",
            wordBreak: "break-all",
          }}
        >
          {logs}
        </pre>
      )}
    </div>
  );
}
