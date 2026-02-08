import puppeteer from "puppeteer";

const BASE = "http://localhost:5174/espidf-bluetooth-typer-preact-app/";

async function run() {
  const browser = await puppeteer.launch({
    headless: true,
    args: ["--no-sandbox", "--disable-setuid-sandbox"],
  });

  const page = await browser.newPage();
  await page.setViewport({ width: 390, height: 844 }); // iPhone 14 size

  const results = [];
  const errors = [];

  page.on("console", (msg) => {
    if (msg.type() === "error") errors.push(msg.text());
  });
  page.on("pageerror", (err) => errors.push(err.message));

  // ---- Test 1: Home page loads ----
  console.log("Test 1: Home page loads...");
  await page.goto(BASE, { waitUntil: "networkidle0" });
  const title = await page.title();
  const h1 = await page.$eval("h1", (el) => el.textContent);
  results.push({
    test: "Home page loads",
    pass: h1 === "ESP32 BLE HID Typer",
    detail: `title="${title}", h1="${h1}"`,
  });

  // ---- Test 2: Four navigation buttons exist ----
  console.log("Test 2: Navigation buttons...");
  const buttons = await page.$$eval("button", (els) =>
    els.map((el) => el.textContent.trim())
  );
  const expectedButtons = [
    "Set Up New Device",
    "Connect to Device",
    "Flash Firmware",
    "Settings",
  ];
  const allFound = expectedButtons.every((b) => buttons.includes(b));
  results.push({
    test: "Navigation buttons present",
    pass: allFound,
    detail: `found: [${buttons.join(", ")}]`,
  });

  // ---- Test 3: Navigate to provisioning ----
  console.log("Test 3: Provisioning screen...");
  await page.click("button >> text=Set Up New Device");
  await page.waitForSelector("h2");
  const provH2 = await page.$eval("h2", (el) => el.textContent);
  results.push({
    test: "Provisioning screen navigates",
    pass: provH2 === "Set Up New Device",
    detail: `h2="${provH2}"`,
  });

  // ---- Test 4: Navigate to Flash Firmware ----
  console.log("Test 4: Flash Firmware screen...");
  await page.goto(BASE + "flash", { waitUntil: "networkidle0" });
  const flashH2 = await page.$eval("h2", (el) => el.textContent);
  results.push({
    test: "Flash Firmware screen loads",
    pass: flashH2 === "Flash Firmware",
    detail: `h2="${flashH2}"`,
  });

  // ---- Test 5: Navigate to Settings ----
  console.log("Test 5: Settings screen...");
  await page.goto(BASE + "settings", { waitUntil: "networkidle0" });
  const settingsH2 = await page.$eval("h2", (el) => el.textContent);
  const hasRange = await page.$("input[type=range]");
  results.push({
    test: "Settings screen loads with controls",
    pass: settingsH2 === "Settings" && hasRange !== null,
    detail: `h2="${settingsH2}", hasRangeInput=${hasRange !== null}`,
  });

  // ---- Test 6: Navigate to PIN change ----
  console.log("Test 6: PIN change screen...");
  await page.goto(BASE + "pin", { waitUntil: "networkidle0" });
  const pinH2 = await page.$eval("h2", (el) => el.textContent);
  const pinInputs = await page.$$("input[type=password]");
  results.push({
    test: "PIN change screen loads",
    pass: pinH2 === "Change PIN" && pinInputs.length === 3,
    detail: `h2="${pinH2}", passwordInputs=${pinInputs.length}`,
  });

  // ---- Test 7: Navigate to Connect ----
  console.log("Test 7: Connect screen...");
  await page.goto(BASE + "connect", { waitUntil: "networkidle0" });
  const connectH2 = await page.$eval("h2", (el) => el.textContent);
  results.push({
    test: "Connect screen loads",
    pass: connectH2 === "Connect to Device",
    detail: `h2="${connectH2}"`,
  });

  // ---- Test 8: Navigate to Audit Log ----
  console.log("Test 8: Audit log screen...");
  await page.goto(BASE + "logs", { waitUntil: "networkidle0" });
  const logsH2 = await page.$eval("h2", (el) => el.textContent);
  results.push({
    test: "Audit log screen loads",
    pass: logsH2 === "Audit Log",
    detail: `h2="${logsH2}"`,
  });

  // ---- Test 9: PWA manifest ----
  console.log("Test 9: PWA manifest...");
  await page.goto(BASE, { waitUntil: "networkidle0" });
  const manifest = await page.evaluate(async () => {
    const link = document.querySelector('link[rel="manifest"]');
    if (!link) return null;
    const resp = await fetch(link.href);
    return resp.json();
  });
  results.push({
    test: "PWA manifest present",
    pass: manifest !== null && manifest.name === "ESP32 BLE HID Typer",
    detail: manifest
      ? `name="${manifest.name}", display="${manifest.display}"`
      : "no manifest found",
  });

  // ---- Test 10: Service worker registered ----
  console.log("Test 10: Service worker...");
  const swRegistered = await page.evaluate(async () => {
    if (!("serviceWorker" in navigator)) return "not supported";
    const regs = await navigator.serviceWorker.getRegistrations();
    return regs.length > 0 ? "registered" : "none";
  });
  results.push({
    test: "Service worker",
    pass: swRegistered === "registered",
    detail: `status=${swRegistered}`,
  });

  // ---- Test 11: No console errors ----
  results.push({
    test: "No JS console errors",
    pass: errors.length === 0,
    detail:
      errors.length > 0 ? `errors: ${errors.join("; ")}` : "clean",
  });

  // ---- Test 12: Take screenshot ----
  console.log("Test 12: Screenshot...");
  await page.goto(BASE, { waitUntil: "networkidle0" });
  await page.screenshot({
    path: "/workspaces/espidf-bluetooth-typer-preact-app/webapp/pwa-screenshot.png",
    fullPage: true,
  });
  results.push({
    test: "Screenshot captured",
    pass: true,
    detail: "pwa-screenshot.png",
  });

  // ---- Report ----
  console.log("\n========== PWA TEST RESULTS ==========\n");
  let passed = 0;
  let failed = 0;
  for (const r of results) {
    const icon = r.pass ? "PASS" : "FAIL";
    console.log(`  [${icon}] ${r.test}`);
    console.log(`         ${r.detail}`);
    if (r.pass) passed++;
    else failed++;
  }
  console.log(`\n  Total: ${passed} passed, ${failed} failed out of ${results.length}\n`);

  await browser.close();
  process.exit(failed > 0 ? 1 : 0);
}

run().catch((err) => {
  console.error("Test runner error:", err);
  process.exit(1);
});
