import { useState } from "preact/hooks";
import { RoutableProps } from "preact-router";
import * as ble from "../utils/ble";
import { validatePin } from "../utils/auth";
import { PageHeader } from "./PageHeader";

export function ProvisioningScreen(_props: RoutableProps) {
  const [step, setStep] = useState<"connect" | "setup" | "done">("connect");
  const [pin, setPin] = useState("");
  const [pinConfirm, setPinConfirm] = useState("");
  const [wifiSsid, setWifiSsid] = useState("");
  const [wifiPassword, setWifiPassword] = useState("");
  const [error, setError] = useState("");
  const [status, setStatus] = useState("");
  const [busy, setBusy] = useState(false);

  const handleConnect = async () => {
    setError("");
    setBusy(true);
    try {
      await ble.scanAndConnect("provisioning");
      setStep("setup");
    } catch (e) {
      setError(e instanceof Error ? e.message : "Connection failed");
    } finally {
      setBusy(false);
    }
  };

  const handleProvision = async () => {
    setError("");

    const pinError = validatePin(pin);
    if (pinError) {
      setError(pinError);
      return;
    }
    if (pin !== pinConfirm) {
      setError("PINs do not match");
      return;
    }

    setBusy(true);
    try {
      setStatus("Setting PIN...");
      const pinResult = await ble.sendProvisioningCommand({
        command: "set_pin",
        pin,
      });
      const pinResp = JSON.parse(pinResult);
      if (!pinResp.success) {
        setError(pinResp.message || "Failed to set PIN");
        setBusy(false);
        return;
      }

      if (wifiSsid) {
        setStatus("Saving WiFi credentials...");
        const wifiResult = await ble.sendProvisioningCommand({
          command: "set_wifi",
          ssid: wifiSsid,
          password: wifiPassword,
        });
        const wifiResp = JSON.parse(wifiResult);
        if (!wifiResp.success) {
          setError(wifiResp.message || "Failed to save WiFi");
          setBusy(false);
          return;
        }
      }

      setStatus("Completing setup...");
      await ble.sendProvisioningCommand({ command: "complete" });
      setStep("done");
    } catch (e) {
      setError(e instanceof Error ? e.message : "Provisioning failed");
    } finally {
      setBusy(false);
      setStatus("");
    }
  };

  if (step === "done") {
    return (
      <div style={{ padding: "2rem", maxWidth: "500px", margin: "0 auto" }}>
        <PageHeader title="Setup Complete" />
        <p style={{ color: "#4ade80" }}>
          Device is rebooting into normal mode. You can now connect via
          "Connect to Device".
        </p>
      </div>
    );
  }

  if (step === "connect") {
    return (
      <div style={{ padding: "2rem", maxWidth: "500px", margin: "0 auto" }}>
        <PageHeader title="Set Up New Device" />
        <p style={{ color: "#94a3b8", marginBottom: "1rem" }}>
          Connect to a device in provisioning mode (orange blinking LED).
        </p>
        <button
          onClick={handleConnect}
          disabled={busy}
          style={{
            padding: "0.75rem 1.5rem",
            background: "#f97316",
            color: "white",
            border: "none",
            borderRadius: "8px",
            fontSize: "1rem",
            cursor: busy ? "not-allowed" : "pointer",
            opacity: busy ? 0.5 : 1,
          }}
        >
          {busy ? "Connecting..." : "Scan for Device"}
        </button>
        {error && <p style={{ color: "#ef4444", marginTop: "1rem" }}>{error}</p>}
      </div>
    );
  }

  return (
    <div style={{ padding: "2rem", maxWidth: "500px", margin: "0 auto" }}>
      <PageHeader title="Device Setup" />

      <div style={{ marginBottom: "1rem" }}>
        <label style={{ display: "block", marginBottom: "0.25rem", color: "#94a3b8" }}>
          PIN (6 digits) *
        </label>
        <input
          type="password"
          inputMode="numeric"
          maxLength={6}
          value={pin}
          onInput={(e) => setPin((e.target as HTMLInputElement).value)}
          style={{
            width: "100%",
            padding: "0.5rem",
            background: "#1e293b",
            border: "1px solid #334155",
            borderRadius: "6px",
            color: "white",
            fontSize: "1.2rem",
            letterSpacing: "0.3rem",
            boxSizing: "border-box",
          }}
        />
      </div>

      <div style={{ marginBottom: "1.5rem" }}>
        <label style={{ display: "block", marginBottom: "0.25rem", color: "#94a3b8" }}>
          Confirm PIN *
        </label>
        <input
          type="password"
          inputMode="numeric"
          maxLength={6}
          value={pinConfirm}
          onInput={(e) => setPinConfirm((e.target as HTMLInputElement).value)}
          style={{
            width: "100%",
            padding: "0.5rem",
            background: "#1e293b",
            border: "1px solid #334155",
            borderRadius: "6px",
            color: "white",
            fontSize: "1.2rem",
            letterSpacing: "0.3rem",
            boxSizing: "border-box",
          }}
        />
      </div>

      <details style={{ marginBottom: "1.5rem" }}>
        <summary style={{ cursor: "pointer", color: "#94a3b8" }}>
          WiFi (optional)
        </summary>
        <div style={{ marginTop: "0.75rem" }}>
          <div style={{ marginBottom: "0.75rem" }}>
            <label style={{ display: "block", marginBottom: "0.25rem", color: "#94a3b8" }}>
              SSID
            </label>
            <input
              type="text"
              value={wifiSsid}
              onInput={(e) => setWifiSsid((e.target as HTMLInputElement).value)}
              style={{
                width: "100%",
                padding: "0.5rem",
                background: "#1e293b",
                border: "1px solid #334155",
                borderRadius: "6px",
                color: "white",
                boxSizing: "border-box",
              }}
            />
          </div>
          <div>
            <label style={{ display: "block", marginBottom: "0.25rem", color: "#94a3b8" }}>
              Password
            </label>
            <input
              type="password"
              value={wifiPassword}
              onInput={(e) => setWifiPassword((e.target as HTMLInputElement).value)}
              style={{
                width: "100%",
                padding: "0.5rem",
                background: "#1e293b",
                border: "1px solid #334155",
                borderRadius: "6px",
                color: "white",
                boxSizing: "border-box",
              }}
            />
          </div>
        </div>
      </details>

      <button
        onClick={handleProvision}
        disabled={busy}
        style={{
          padding: "0.75rem 1.5rem",
          background: "#f97316",
          color: "white",
          border: "none",
          borderRadius: "8px",
          fontSize: "1rem",
          cursor: busy ? "not-allowed" : "pointer",
          opacity: busy ? 0.5 : 1,
          width: "100%",
        }}
      >
        {busy ? status || "Working..." : "Complete Setup"}
      </button>

      {error && <p style={{ color: "#ef4444", marginTop: "1rem" }}>{error}</p>}
    </div>
  );
}
