import { useState } from "preact/hooks";
import * as ble from "../utils/ble";

interface Props {
  disabled?: boolean;
}

export function ClipboardPaste({ disabled }: Props) {
  const [sending, setSending] = useState(false);
  const [error, setError] = useState("");

  const handlePaste = async () => {
    setError("");
    try {
      const text = await navigator.clipboard.readText();
      if (!text) {
        setError("Clipboard is empty");
        return;
      }
      setSending(true);
      await ble.sendText(text);
    } catch (e) {
      setError(
        e instanceof Error ? e.message : "Failed to read clipboard"
      );
    } finally {
      setSending(false);
    }
  };

  return (
    <div>
      <button
        onClick={handlePaste}
        disabled={disabled || sending}
        style={{
          height: "2.5rem",
          padding: "0 1rem",
          background: "#6366f1",
          color: "white",
          border: "none",
          borderRadius: "6px",
          cursor: disabled || sending ? "not-allowed" : "pointer",
          opacity: disabled || sending ? 0.5 : 1,
          fontSize: "1rem",
          display: "inline-flex",
          alignItems: "center",
        }}
      >
        {sending ? "Sending..." : "Paste & Send"}
      </button>
      {error && (
        <p style={{ color: "#ef4444", fontSize: "0.85rem", marginTop: "0.5rem" }}>
          {error}
        </p>
      )}
    </div>
  );
}
