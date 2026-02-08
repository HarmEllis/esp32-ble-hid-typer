import { useState, useEffect } from "preact/hooks";
import { RoutableProps } from "preact-router";
import * as ble from "../utils/ble";
import { StatusBar } from "./StatusBar";
import { ClipboardPaste } from "./ClipboardPaste";
import { nav } from "../utils/nav";

export function TextSender(_props: RoutableProps) {
  const [text, setText] = useState("");
  const [sending, setSending] = useState(false);
  const [error, setError] = useState("");
  const [connected, setConnected] = useState(true);

  useEffect(() => {
    if (!ble.isConnected()) {
      nav("/connect");
      return;
    }
    ble.onDisconnect(() => {
      setConnected(false);
    });
  }, []);

  const handleSend = async () => {
    if (!text.trim()) return;
    setError("");
    setSending(true);
    try {
      await ble.sendText(text);
      setText("");
    } catch (e) {
      setError(e instanceof Error ? e.message : "Failed to send text");
    } finally {
      setSending(false);
    }
  };

  const handleAbort = async () => {
    try {
      await ble.sendPinAction({ action: "abort" });
    } catch {
      /* ignore */
    }
  };

  const handleDisconnect = async () => {
    await ble.disconnect();
    nav("/");
  };

  if (!connected) {
    return (
      <div style={{ padding: "2rem", maxWidth: "600px", margin: "0 auto" }}>
        <h2>Disconnected</h2>
        <p style={{ color: "#ef4444" }}>Device disconnected.</p>
        <button
          onClick={() => nav("/connect")}
          style={{
            padding: "0.5rem 1rem",
            background: "#3b82f6",
            color: "white",
            border: "none",
            borderRadius: "6px",
            cursor: "pointer",
            marginTop: "1rem",
          }}
        >
          Reconnect
        </button>
      </div>
    );
  }

  return (
    <div style={{ padding: "2rem", maxWidth: "600px", margin: "0 auto" }}>
      <div
        style={{
          display: "flex",
          justifyContent: "space-between",
          alignItems: "center",
          marginBottom: "1rem",
        }}
      >
        <h2 style={{ margin: 0 }}>Send Text</h2>
        <button
          onClick={handleDisconnect}
          style={{
            padding: "0.25rem 0.75rem",
            background: "transparent",
            color: "#94a3b8",
            border: "1px solid #334155",
            borderRadius: "6px",
            cursor: "pointer",
            fontSize: "0.85rem",
          }}
        >
          Disconnect
        </button>
      </div>

      <StatusBar />

      <textarea
        value={text}
        onInput={(e) => setText((e.target as HTMLTextAreaElement).value)}
        placeholder="Type or paste text to send..."
        style={{
          width: "100%",
          minHeight: "150px",
          padding: "0.75rem",
          background: "#1e293b",
          border: "1px solid #334155",
          borderRadius: "8px",
          color: "white",
          fontSize: "1rem",
          resize: "vertical",
          fontFamily: "monospace",
          boxSizing: "border-box",
        }}
      />

      <div
        style={{
          display: "flex",
          gap: "0.5rem",
          marginTop: "1rem",
          flexWrap: "wrap",
        }}
      >
        <button
          onClick={handleSend}
          disabled={sending || !text.trim()}
          style={{
            padding: "0.5rem 1.5rem",
            background: "#3b82f6",
            color: "white",
            border: "none",
            borderRadius: "6px",
            cursor: sending || !text.trim() ? "not-allowed" : "pointer",
            opacity: sending || !text.trim() ? 0.5 : 1,
            fontSize: "1rem",
          }}
        >
          {sending ? "Sending..." : "Send"}
        </button>

        <ClipboardPaste disabled={sending} />

        <button
          onClick={handleAbort}
          style={{
            padding: "0.5rem 1rem",
            background: "#dc2626",
            color: "white",
            border: "none",
            borderRadius: "6px",
            cursor: "pointer",
          }}
        >
          Abort
        </button>
      </div>

      {error && <p style={{ color: "#ef4444", marginTop: "1rem" }}>{error}</p>}

      <div style={{ marginTop: "2rem" }}>
        <button
          onClick={() => nav("/settings")}
          style={{
            padding: "0.25rem 0.75rem",
            background: "transparent",
            color: "#64748b",
            border: "1px solid #334155",
            borderRadius: "6px",
            cursor: "pointer",
            fontSize: "0.85rem",
          }}
        >
          Settings
        </button>
      </div>
    </div>
  );
}
