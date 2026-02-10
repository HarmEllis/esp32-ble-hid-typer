import { useState, useEffect } from "preact/hooks";
import { RoutableProps } from "preact-router";
import * as ble from "../utils/ble";
import { StatusBar } from "./StatusBar";
import { ClipboardPaste } from "./ClipboardPaste";
import { VirtualKeyboard, type VirtualSpecialKey } from "./VirtualKeyboard";
import { nav } from "../utils/nav";

const CTRL_ALT_MODIFIER = 0x01 | 0x04;
const CTRL_MODIFIER = 0x01;
const CTRL_ALT_FUNCTION_SHORTCUTS = Array.from({ length: 12 }, (_, index) => ({
  label: `Ctrl+Alt+F${index + 1}`,
  modifier: CTRL_ALT_MODIFIER,
  keycode: 0x3a + index,
}));
const COMMON_CTRL_SHORTCUTS = [
  { label: "Ctrl+A", modifier: CTRL_MODIFIER, keycode: 0x04 },
  { label: "Ctrl+C", modifier: CTRL_MODIFIER, keycode: 0x06 },
  { label: "Ctrl+V", modifier: CTRL_MODIFIER, keycode: 0x19 },
  { label: "Ctrl+Enter", modifier: CTRL_MODIFIER, keycode: 0x28 },
];
const NAVIGATION_KEYS = [
  { label: "Left", keycode: 0x50 },
  { label: "Right", keycode: 0x4f },
  { label: "Home", keycode: 0x4a },
  { label: "End", keycode: 0x4d },
];

export function TextSender(_props: RoutableProps) {
  const [text, setText] = useState("");
  const [sending, setSending] = useState(false);
  const [sendingSpecial, setSendingSpecial] = useState(false);
  const [error, setError] = useState("");
  const [connected, setConnected] = useState(true);
  const [checkingAuth, setCheckingAuth] = useState(true);
  const [keyboardConnected, setKeyboardConnected] = useState(true);

  useEffect(() => {
    if (!ble.isConnected()) {
      nav("/connect");
      return;
    }

    ble.readStatusObject().then((status) => {
      if (!status.authenticated) {
        nav("/connect");
        return;
      }
      setKeyboardConnected(status.keyboard_connected !== false);
      setCheckingAuth(false);
    }).catch(() => {
      nav("/connect");
    });

    ble.onStatusChange((value) => {
      try {
        const status = JSON.parse(value) as { keyboard_connected?: boolean };
        if (typeof status.keyboard_connected === "boolean") {
          setKeyboardConnected(status.keyboard_connected);
        }
      } catch {
        /* ignore parse errors */
      }
    }).catch(() => {
      /* subscription can fail during reconnect transitions */
    });

    const interval = window.setInterval(() => {
      ble.readStatusObject().then((status) => {
        setKeyboardConnected(status.keyboard_connected !== false);
      }).catch(() => {
        /* ignore transient read failures */
      });
    }, 1000);

    ble.onDisconnect(() => {
      setConnected(false);
    });

    return () => {
      window.clearInterval(interval);
    };
  }, []);

  const ensureKeyboardConnected = async () => {
    try {
      const status = await ble.readStatusObject();
      const ready = status.keyboard_connected !== false;
      setKeyboardConnected(ready);
      if (!ready) {
        setError("USB keyboard is not connected. Attach the ESP32 to a host first.");
      }
      return ready;
    } catch {
      setError("Failed to read keyboard connection status");
      return false;
    }
  };

  const handleSend = async () => {
    if (!text.trim()) return;
    setError("");
    if (!(await ensureKeyboardConnected())) return;
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

  const handleSpecialKey = async (payload: string) => {
    if (sendingSpecial || sending) return;
    setError("");
    if (!(await ensureKeyboardConnected())) return;
    setSendingSpecial(true);
    try {
      await ble.sendText(payload);
    } catch (e) {
      setError(e instanceof Error ? e.message : "Failed to send special key");
    } finally {
      setSendingSpecial(false);
    }
  };

  const handleShortcut = async (modifier: number, keycode: number) => {
    if (sendingSpecial || sending) return;
    setError("");
    if (!(await ensureKeyboardConnected())) return;
    setSendingSpecial(true);
    try {
      await ble.sendPinAction({ action: "key_combo", modifier, keycode });
    } catch (e) {
      setError(e instanceof Error ? e.message : "Failed to send shortcut");
    } finally {
      setSendingSpecial(false);
    }
  };

  const handleVirtualSpecialKey = async (key: VirtualSpecialKey) => {
    if (key === "backspace") {
      await handleSpecialKey("\b");
      return;
    }
    if (key === "tab") {
      await handleSpecialKey("\t");
      return;
    }
    if (key === "enter") {
      await handleSpecialKey("\n");
      return;
    }
    if (key === "left") {
      await handleShortcut(0, 0x50);
      return;
    }
    if (key === "right") {
      await handleShortcut(0, 0x4f);
      return;
    }
    if (key === "home") {
      await handleShortcut(0, 0x4a);
      return;
    }
    await handleShortcut(0, 0x4d);
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

  if (checkingAuth) {
    return (
      <div style={{ padding: "2rem", maxWidth: "600px", margin: "0 auto" }}>
        <h2>Authorizing</h2>
        <p style={{ color: "#94a3b8" }}>Checking device unlock state...</p>
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
        <div style={{ display: "flex", alignItems: "center", gap: "0.75rem" }}>
          <button
            onClick={() => nav("/connect")}
            style={{
              background: "none",
              border: "none",
              color: "#94a3b8",
              cursor: "pointer",
              fontSize: "1.2rem",
              padding: "0.25rem",
              lineHeight: 1,
            }}
            aria-label="Go back"
          >
            &larr;
          </button>
          <h2 style={{ margin: 0 }}>Send Text</h2>
        </div>
        <div style={{ display: "flex", alignItems: "center", gap: "0.5rem" }}>
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
          disabled={sending || sendingSpecial || !keyboardConnected || !text.trim()}
          style={{
            padding: "0.5rem 1.5rem",
            background: "#3b82f6",
            color: "white",
            border: "none",
            borderRadius: "6px",
            cursor:
              sending || sendingSpecial || !keyboardConnected || !text.trim()
                ? "not-allowed"
                : "pointer",
            opacity: sending || sendingSpecial || !keyboardConnected || !text.trim() ? 0.5 : 1,
            fontSize: "1rem",
          }}
        >
          {sending ? "Sending..." : "Send"}
        </button>

        <ClipboardPaste disabled={sending || sendingSpecial || !keyboardConnected} />

        <button
          onClick={handleAbort}
          disabled={sendingSpecial}
          style={{
            padding: "0.5rem 1rem",
            background: "#dc2626",
            color: "white",
            border: "none",
            borderRadius: "6px",
            cursor: sendingSpecial ? "not-allowed" : "pointer",
            opacity: sendingSpecial ? 0.5 : 1,
          }}
        >
          Abort
        </button>
      </div>

      <div
        style={{
          display: "flex",
          gap: "0.5rem",
          marginTop: "0.75rem",
          flexWrap: "wrap",
        }}
      >
        <button
          onClick={() => handleSpecialKey("\b")}
          disabled={sending || sendingSpecial || !keyboardConnected}
          style={{
            padding: "0.45rem 0.9rem",
            background: "#374151",
            color: "white",
            border: "none",
            borderRadius: "6px",
            cursor: sending || sendingSpecial || !keyboardConnected ? "not-allowed" : "pointer",
            opacity: sending || sendingSpecial || !keyboardConnected ? 0.5 : 1,
          }}
        >
          Backspace
        </button>
        <button
          onClick={() => handleSpecialKey("\u007f")}
          disabled={sending || sendingSpecial || !keyboardConnected}
          style={{
            padding: "0.45rem 0.9rem",
            background: "#374151",
            color: "white",
            border: "none",
            borderRadius: "6px",
            cursor: sending || sendingSpecial || !keyboardConnected ? "not-allowed" : "pointer",
            opacity: sending || sendingSpecial || !keyboardConnected ? 0.5 : 1,
          }}
        >
          Delete
        </button>
        <button
          onClick={() => handleSpecialKey("\n")}
          disabled={sending || sendingSpecial || !keyboardConnected}
          style={{
            padding: "0.45rem 0.9rem",
            background: "#374151",
            color: "white",
            border: "none",
            borderRadius: "6px",
            cursor: sending || sendingSpecial || !keyboardConnected ? "not-allowed" : "pointer",
            opacity: sending || sendingSpecial || !keyboardConnected ? 0.5 : 1,
          }}
        >
          Enter
        </button>
        {NAVIGATION_KEYS.map((key) => (
          <button
            key={key.label}
            onClick={() => handleShortcut(0, key.keycode)}
            disabled={sending || sendingSpecial || !keyboardConnected}
            style={{
              padding: "0.45rem 0.9rem",
              background: "#374151",
              color: "white",
              border: "none",
              borderRadius: "6px",
              cursor: sending || sendingSpecial || !keyboardConnected ? "not-allowed" : "pointer",
              opacity: sending || sendingSpecial || !keyboardConnected ? 0.5 : 1,
            }}
          >
            {key.label}
          </button>
        ))}
      </div>

      <details style={{ marginTop: "1rem" }}>
        <summary style={{ cursor: "pointer", color: "#94a3b8" }}>
          Shortcuts
        </summary>
        <div
          style={{
            marginTop: "0.75rem",
            display: "grid",
            gridTemplateColumns: "repeat(auto-fit, minmax(140px, 1fr))",
            gap: "0.5rem",
          }}
        >
          {COMMON_CTRL_SHORTCUTS.map((shortcut) => (
            <button
              key={shortcut.label}
              onClick={() => handleShortcut(shortcut.modifier, shortcut.keycode)}
              disabled={sending || sendingSpecial || !keyboardConnected}
              style={{
                padding: "0.45rem 0.75rem",
                background: "#334155",
                color: "#e2e8f0",
                border: "1px solid #475569",
                borderRadius: "6px",
                cursor: sending || sendingSpecial || !keyboardConnected ? "not-allowed" : "pointer",
                opacity: sending || sendingSpecial || !keyboardConnected ? 0.5 : 1,
              }}
            >
              {shortcut.label}
            </button>
          ))}
        </div>
        <div
          style={{
            marginTop: "0.75rem",
            display: "grid",
            gridTemplateColumns: "repeat(auto-fit, minmax(140px, 1fr))",
            gap: "0.5rem",
          }}
        >
          {CTRL_ALT_FUNCTION_SHORTCUTS.map((shortcut) => (
            <button
              key={shortcut.label}
              onClick={() => handleShortcut(shortcut.modifier, shortcut.keycode)}
              disabled={sending || sendingSpecial || !keyboardConnected}
              style={{
                padding: "0.45rem 0.75rem",
                background: "#1e293b",
                color: "#cbd5e1",
                border: "1px solid #334155",
                borderRadius: "6px",
                cursor: sending || sendingSpecial || !keyboardConnected ? "not-allowed" : "pointer",
                opacity: sending || sendingSpecial || !keyboardConnected ? 0.5 : 1,
              }}
            >
              {shortcut.label}
            </button>
          ))}
        </div>
      </details>

      <details style={{ marginTop: "1rem" }}>
        <summary style={{ cursor: "pointer", color: "#94a3b8" }}>
          Virtual Keyboard
        </summary>
        <VirtualKeyboard
          disabled={sending || sendingSpecial || !keyboardConnected}
          onTextKey={handleSpecialKey}
          onSpecialKey={handleVirtualSpecialKey}
        />
      </details>

      {!keyboardConnected && (
        <p style={{ color: "#f97316", marginTop: "1rem" }}>
          USB keyboard is not mounted on a host, so send actions are disabled.
        </p>
      )}

      {error && <p style={{ color: "#ef4444", marginTop: "1rem" }}>{error}</p>}
    </div>
  );
}
