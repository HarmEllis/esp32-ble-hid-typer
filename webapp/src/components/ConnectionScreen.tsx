import { RoutableProps } from "preact-router";
import { nav } from "../utils/nav";

export function ConnectionScreen(_props: RoutableProps) {
  return (
    <div style={{ padding: "2rem", maxWidth: "500px", margin: "0 auto" }}>
      <h1 style={{ marginBottom: "2rem" }}>ESP32 BLE HID Typer</h1>

      <div style={{ display: "flex", flexDirection: "column", gap: "1rem" }}>
        <button
          onClick={() => nav("/provision")}
          style={{
            padding: "1rem",
            background: "#f97316",
            color: "white",
            border: "none",
            borderRadius: "8px",
            fontSize: "1rem",
            cursor: "pointer",
          }}
        >
          Set Up New Device
        </button>

        <button
          onClick={() => nav("/connect")}
          style={{
            padding: "1rem",
            background: "#3b82f6",
            color: "white",
            border: "none",
            borderRadius: "8px",
            fontSize: "1rem",
            cursor: "pointer",
          }}
        >
          Connect to Device
        </button>

        <button
          onClick={() => nav("/flash")}
          style={{
            padding: "1rem",
            background: "#6b7280",
            color: "white",
            border: "none",
            borderRadius: "8px",
            fontSize: "1rem",
            cursor: "pointer",
          }}
        >
          Flash Firmware
        </button>

        <button
          onClick={() => nav("/settings")}
          style={{
            padding: "1rem",
            background: "#1e293b",
            color: "#94a3b8",
            border: "1px solid #334155",
            borderRadius: "8px",
            fontSize: "1rem",
            cursor: "pointer",
          }}
        >
          Settings
        </button>
      </div>
    </div>
  );
}
