import { useState } from "preact/hooks";
import { RoutableProps } from "preact-router";
import { validatePin } from "../utils/auth";
import * as ble from "../utils/ble";
import { PageHeader } from "./PageHeader";

export function PinSetup(_props: RoutableProps) {
  const [oldPin, setOldPin] = useState("");
  const [newPin, setNewPin] = useState("");
  const [confirmPin, setConfirmPin] = useState("");
  const [error, setError] = useState("");
  const [success, setSuccess] = useState("");
  const [busy, setBusy] = useState(false);

  const handleChange = async () => {
    setError("");
    setSuccess("");

    if (!oldPin) {
      setError("Enter current PIN");
      return;
    }
    const pinError = validatePin(newPin);
    if (pinError) {
      setError(pinError);
      return;
    }
    if (newPin !== confirmPin) {
      setError("New PINs do not match");
      return;
    }

    setBusy(true);
    try {
      await ble.sendPinAction({ action: "set", old: oldPin, new: newPin });
      setSuccess("PIN changed successfully");
      setOldPin("");
      setNewPin("");
      setConfirmPin("");
    } catch (e) {
      setError(e instanceof Error ? e.message : "Failed to change PIN");
    } finally {
      setBusy(false);
    }
  };

  const inputStyle = {
    width: "100%",
    padding: "0.5rem",
    background: "#1e293b",
    border: "1px solid #334155",
    borderRadius: "6px",
    color: "white",
    fontSize: "1.2rem",
    letterSpacing: "0.3rem",
    boxSizing: "border-box" as const,
  };

  return (
    <div style={{ padding: "2rem", maxWidth: "500px", margin: "0 auto" }}>
      <PageHeader title="Change PIN" backTo="/send" />

      <div style={{ marginBottom: "1rem" }}>
        <label style={{ display: "block", marginBottom: "0.25rem", color: "#94a3b8" }}>
          Current PIN
        </label>
        <input
          type="password"
          inputMode="numeric"
          maxLength={6}
          value={oldPin}
          onInput={(e) => setOldPin((e.target as HTMLInputElement).value)}
          style={inputStyle}
        />
      </div>

      <div style={{ marginBottom: "1rem" }}>
        <label style={{ display: "block", marginBottom: "0.25rem", color: "#94a3b8" }}>
          New PIN (6 digits)
        </label>
        <input
          type="password"
          inputMode="numeric"
          maxLength={6}
          value={newPin}
          onInput={(e) => setNewPin((e.target as HTMLInputElement).value)}
          style={inputStyle}
        />
      </div>

      <div style={{ marginBottom: "1.5rem" }}>
        <label style={{ display: "block", marginBottom: "0.25rem", color: "#94a3b8" }}>
          Confirm New PIN
        </label>
        <input
          type="password"
          inputMode="numeric"
          maxLength={6}
          value={confirmPin}
          onInput={(e) => setConfirmPin((e.target as HTMLInputElement).value)}
          style={inputStyle}
        />
      </div>

      <button
        onClick={handleChange}
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
          width: "100%",
        }}
      >
        {busy ? "Changing..." : "Change PIN"}
      </button>

      {error && <p style={{ color: "#ef4444", marginTop: "1rem" }}>{error}</p>}
      {success && <p style={{ color: "#4ade80", marginTop: "1rem" }}>{success}</p>}
    </div>
  );
}
