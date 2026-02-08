/* localStorage wrapper for app settings */

const PREFIX = "hid-typer-";

function getItem(key: string): string | null {
  try {
    return localStorage.getItem(PREFIX + key);
  } catch {
    return null;
  }
}

function setItem(key: string, value: string): void {
  try {
    localStorage.setItem(PREFIX + key, value);
  } catch {
    /* Ignore storage errors */
  }
}

export function getSysRqEnabled(): boolean {
  return getItem("sysrq-enabled") === "true";
}

export function setSysRqEnabled(enabled: boolean): void {
  setItem("sysrq-enabled", String(enabled));
}

export function getTypingDelay(): number {
  const val = getItem("typing-delay");
  return val ? Number(val) : 10;
}

export function setTypingDelay(ms: number): void {
  setItem("typing-delay", String(ms));
}

export function getLedBrightness(): number {
  const val = getItem("led-brightness");
  return val ? Number(val) : 5;
}

export function setLedBrightness(percent: number): void {
  setItem("led-brightness", String(percent));
}
