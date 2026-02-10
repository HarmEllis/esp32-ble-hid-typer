import { useEffect, useState } from "preact/hooks";
import { RoutableProps } from "preact-router";
import * as ble from "../utils/ble";
import * as storage from "../utils/storage";
import { nav } from "../utils/nav";
import { PageHeader } from "./PageHeader";

export function Settings(_props: RoutableProps) {
  const [typingDelay, setTypingDelay] = useState(storage.getTypingDelay());
  const [ledBrightness, setLedBrightness] = useState(storage.getLedBrightness());
  const [sysrqEnabled, setSysrqEnabled] = useState(storage.getSysRqEnabled());
  const [status, setStatus] = useState("");
  const [connected, setConnected] = useState(false);
  const [checkingAccess, setCheckingAccess] = useState(true);

  useEffect(() => {
    if (!ble.isConnected()) {
      nav("/connect");
      return;
    }

    ble
      .readStatusObject()
      .then((deviceStatus) => {
        if (!deviceStatus.authenticated) {
          nav("/connect");
          return;
        }
        setConnected(true);
        setCheckingAccess(false);
      })
      .catch(() => {
        nav("/connect");
      });

    ble.onDisconnect(() => {
      setConnected(false);
      nav("/connect");
    });
  }, []);

  const handleTypingDelayChange = async (ms: number) => {
    if (!connected) {
      setStatus("Device is not connected");
      return;
    }

    try {
      await ble.sendPinAction({
        action: "set_config",
        key: "typing_delay",
        value: String(ms),
      });
      setTypingDelay(ms);
      storage.setTypingDelay(ms);
      setStatus("Typing delay updated");
    } catch {
      setStatus("Failed to update typing delay");
    }
  };

  const handleBrightnessChange = async (percent: number) => {
    if (!connected) {
      setStatus("Device is not connected");
      return;
    }

    try {
      await ble.sendPinAction({
        action: "set_config",
        key: "led_brightness",
        value: String(percent),
      });
      setLedBrightness(percent);
      storage.setLedBrightness(percent);
      setStatus("LED brightness updated");
    } catch {
      setStatus("Failed to update LED brightness");
    }
  };

  const handleSysrqToggle = (enabled: boolean) => {
    setSysrqEnabled(enabled);
    storage.setSysRqEnabled(enabled);
  };

  if (checkingAccess) {
    return (
      <div style={{ padding: "2rem", maxWidth: "500px", margin: "0 auto" }}>
        <PageHeader title="Settings" />
        <p style={{ color: "#94a3b8" }}>Checking device access...</p>
      </div>
    );
  }

  return (
    <div style={{ padding: "2rem", maxWidth: "500px", margin: "0 auto" }}>
      <PageHeader title="Settings" />

      <div style={{ marginBottom: "1.5rem" }}>
        <label style={{ display: "block", marginBottom: "0.5rem", color: "#94a3b8" }}>
          Typing Delay: {typingDelay}ms
        </label>
        <input
          type="range"
          min={5}
          max={100}
          value={typingDelay}
          disabled={!connected}
          onInput={(e) =>
            handleTypingDelayChange(Number((e.target as HTMLInputElement).value))
          }
          style={{ width: "100%" }}
        />
        <div
          style={{
            display: "flex",
            justifyContent: "space-between",
            fontSize: "0.75rem",
            color: "#64748b",
          }}
        >
          <span>5ms (fast)</span>
          <span>100ms (slow)</span>
        </div>
      </div>

      <div style={{ marginBottom: "1.5rem" }}>
        <label style={{ display: "block", marginBottom: "0.5rem", color: "#94a3b8" }}>
          LED Brightness: {ledBrightness}%
        </label>
        <input
          type="range"
          min={1}
          max={100}
          value={ledBrightness}
          disabled={!connected}
          onInput={(e) =>
            handleBrightnessChange(Number((e.target as HTMLInputElement).value))
          }
          style={{ width: "100%" }}
        />
      </div>

      <div style={{ marginBottom: "1.5rem" }}>
        <label
          style={{
            display: "flex",
            alignItems: "center",
            gap: "0.5rem",
            color: "#94a3b8",
            cursor: "pointer",
          }}
        >
          <input
            type="checkbox"
            checked={sysrqEnabled}
            onChange={(e) =>
              handleSysrqToggle((e.target as HTMLInputElement).checked)
            }
          />
          Enable SysRq commands (advanced)
        </label>
        {sysrqEnabled && (
          <p
            style={{
              color: "#f97316",
              fontSize: "0.85rem",
              marginTop: "0.5rem",
            }}
          >
            SysRq commands can send kernel-level commands to the target machine.
            Use with caution.
          </p>
        )}
      </div>

      <div
        style={{
          display: "flex",
          gap: "0.5rem",
          flexWrap: "wrap",
          marginTop: "2rem",
          borderTop: "1px solid #334155",
          paddingTop: "1.5rem",
        }}
      >
        <button
          onClick={() => nav("/pin")}
          style={{
            padding: "0.5rem 1rem",
            background: "#1e293b",
            color: "#94a3b8",
            border: "1px solid #334155",
            borderRadius: "6px",
            cursor: "pointer",
          }}
        >
          Change PIN
        </button>
        <button
          onClick={() => nav("/logs")}
          style={{
            padding: "0.5rem 1rem",
            background: "#1e293b",
            color: "#94a3b8",
            border: "1px solid #334155",
            borderRadius: "6px",
            cursor: "pointer",
          }}
        >
          Audit Log
        </button>
      </div>

      {status && (
        <p style={{ color: "#4ade80", fontSize: "0.85rem", marginTop: "1rem" }}>
          {status}
        </p>
      )}
    </div>
  );
}
