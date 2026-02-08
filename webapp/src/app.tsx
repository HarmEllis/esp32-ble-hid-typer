import Router from "preact-router";
import { useEffect } from "preact/hooks";
import { ConnectionScreen } from "./components/ConnectionScreen";
import { ProvisioningScreen } from "./components/ProvisioningScreen";
import { BleConnect } from "./components/BleConnect";
import { TextSender } from "./components/TextSender";
import { PinSetup } from "./components/PinSetup";
import { FirmwareFlash } from "./components/FirmwareFlash";
import { Settings } from "./components/Settings";
import { AuditLog } from "./components/AuditLog";
import { BASE, nav } from "./utils/nav";

function Redirect(_props: { path?: string; default?: boolean }) {
  useEffect(() => {
    nav("/");
  }, []);
  return null;
}

export function App() {
  return (
    <div
      style={{
        minHeight: "100vh",
        background: "#1a1a2e",
        color: "#e2e8f0",
        fontFamily: "system-ui, sans-serif",
      }}
    >
      <Router>
        <ConnectionScreen path={`${BASE}/`} />
        <ProvisioningScreen path={`${BASE}/provision`} />
        <BleConnect path={`${BASE}/connect`} />
        <TextSender path={`${BASE}/send`} />
        <PinSetup path={`${BASE}/pin`} />
        <FirmwareFlash path={`${BASE}/flash`} />
        <Settings path={`${BASE}/settings`} />
        <AuditLog path={`${BASE}/logs`} />
        <Redirect default />
      </Router>
    </div>
  );
}
