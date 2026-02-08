import { useState } from "preact/hooks";
import { RoutableProps } from "preact-router";

export function FirmwareFlash(_props: RoutableProps) {
  const [status, setStatus] = useState("");
  const [error, setError] = useState("");
  const [progress, setProgress] = useState(0);
  const [flashing, setFlashing] = useState(false);

  const handleFlash = async () => {
    setError("");
    setStatus("");
    setProgress(0);

    if (!("serial" in navigator)) {
      setError(
        "Web Serial API not supported. Use Chrome, Edge, or Opera."
      );
      return;
    }

    try {
      setStatus("Select the serial port for your ESP32...");
      const port = await (navigator as any).serial.requestPort();
      await port.open({ baudRate: 115200 });

      setStatus(
        "Serial port opened. Use the esptool-js flash tool to flash firmware."
      );

      /* For now, show instructions. Full esptool-js integration is a
         Phase 3 enhancement that requires the firmware binary to be
         available (e.g. from a GitHub Release). */
      setStatus(
        "Serial port connected. Full firmware flashing with esptool-js " +
          "will be available when firmware binaries are published to GitHub Releases."
      );

      await port.close();
    } catch (e) {
      setError(e instanceof Error ? e.message : "Flash failed");
    } finally {
      setFlashing(false);
    }
  };

  return (
    <div style={{ padding: "2rem", maxWidth: "500px", margin: "0 auto" }}>
      <h2>Flash Firmware</h2>
      <p style={{ color: "#94a3b8", marginBottom: "1rem" }}>
        Flash firmware to your ESP32-S3 via USB. Connect the device with a USB
        cable and hold the BOOT button while pressing RESET to enter download
        mode.
      </p>

      <button
        onClick={handleFlash}
        disabled={flashing}
        style={{
          padding: "0.75rem 1.5rem",
          background: "#6b7280",
          color: "white",
          border: "none",
          borderRadius: "8px",
          fontSize: "1rem",
          cursor: flashing ? "not-allowed" : "pointer",
          opacity: flashing ? 0.5 : 1,
        }}
      >
        {flashing ? `Flashing... ${progress}%` : "Connect & Flash"}
      </button>

      {status && (
        <p style={{ color: "#94a3b8", marginTop: "1rem" }}>{status}</p>
      )}
      {error && (
        <p style={{ color: "#ef4444", marginTop: "1rem" }}>{error}</p>
      )}
    </div>
  );
}
