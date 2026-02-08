import { useState, useRef, useCallback } from "preact/hooks";
import { RoutableProps } from "preact-router";
import { ESPLoader, Transport, FlashOptions } from "esptool-js";
import { nav } from "../utils/nav";

/** ESP32-S3 flash offsets */
const OFFSETS = {
  bootloader: 0x0,
  partitionTable: 0x8000,
  firmware: 0x20000,
} as const;

type FlashSource = "files" | "release";

interface FileSlot {
  label: string;
  offset: number;
  file: File | null;
  data: string | null;
  required: boolean;
}

/** Convert ArrayBuffer to binary string (as esptool-js expects) */
function arrayBufferToBstr(buf: ArrayBuffer): string {
  const bytes = new Uint8Array(buf);
  const chunks: string[] = [];
  // Process in chunks to avoid call stack size limits
  const chunkSize = 8192;
  for (let i = 0; i < bytes.length; i += chunkSize) {
    const slice = bytes.subarray(i, i + chunkSize);
    chunks.push(String.fromCharCode(...slice));
  }
  return chunks.join("");
}

/**
 * Derive GitHub owner/repo from the current page URL when hosted on
 * GitHub Pages (https://<owner>.github.io/<repo>/).
 */
function detectGitHubRepo(): { owner: string; repo: string } | null {
  const { hostname, pathname } = window.location;
  const match = hostname.match(/^(.+)\.github\.io$/);
  if (!match) return null;
  const owner = match[1];
  const repo = pathname.split("/").filter(Boolean)[0];
  if (!repo) return null;
  return { owner, repo };
}

export function FirmwareFlash(_props: RoutableProps) {
  const [source, setSource] = useState<FlashSource>("files");
  const [status, setStatus] = useState("");
  const [error, setError] = useState("");
  const [flashing, setFlashing] = useState(false);
  const [connected, setConnected] = useState(false);
  const [chipInfo, setChipInfo] = useState("");
  const [fileProgress, setFileProgress] = useState<number[]>([]);
  const [logLines, setLogLines] = useState<string[]>([]);

  const [releaseUrl, setReleaseUrl] = useState("");
  const [releases, setReleases] = useState<
    { tag: string; url: string }[] | null
  >(null);
  const [loadingReleases, setLoadingReleases] = useState(false);
  const [selectedTag, setSelectedTag] = useState("");

  const loaderRef = useRef<ESPLoader | null>(null);
  const transportRef = useRef<Transport | null>(null);

  const [slots, setSlots] = useState<FileSlot[]>([
    {
      label: "Bootloader",
      offset: OFFSETS.bootloader,
      file: null,
      data: null,
      required: true,
    },
    {
      label: "Partition Table",
      offset: OFFSETS.partitionTable,
      file: null,
      data: null,
      required: true,
    },
    {
      label: "Firmware",
      offset: OFFSETS.firmware,
      file: null,
      data: null,
      required: true,
    },
  ]);

  const logRef = useRef<HTMLDivElement>(null);

  const appendLog = useCallback((line: string) => {
    setLogLines((prev) => {
      const next = [...prev, line];
      // Keep last 200 lines
      return next.length > 200 ? next.slice(-200) : next;
    });
    requestAnimationFrame(() => {
      logRef.current?.scrollTo(0, logRef.current.scrollHeight);
    });
  }, []);

  const terminal = {
    clean: () => setLogLines([]),
    writeLine: (data: string) => appendLog(data),
    write: (data: string) => appendLog(data),
  };

  /** Read a File into a binary string */
  async function readFileAsBstr(file: File): Promise<string> {
    const buf = await file.arrayBuffer();
    return arrayBufferToBstr(buf);
  }

  /** Set a file for a given slot index */
  function handleFileSelect(index: number, file: File | null) {
    setSlots((prev) =>
      prev.map((s, i) => (i === index ? { ...s, file, data: null } : s))
    );
  }

  /** Connect to the ESP32 via Web Serial */
  async function handleConnect() {
    setError("");
    setStatus("Select the serial port for your ESP32...");
    setLogLines([]);

    if (!("serial" in navigator)) {
      setError("Web Serial API not supported. Use Chrome, Edge, or Opera.");
      return;
    }

    try {
      const port = await navigator.serial.requestPort();
      const transport = new Transport(port, true);
      transportRef.current = transport;

      const loader = new ESPLoader({
        transport,
        baudrate: 921600,
        romBaudrate: 115200,
        terminal,
      });
      loaderRef.current = loader;

      setStatus("Connecting to ESP32...");
      const chip = await loader.main();
      setChipInfo(chip);
      setConnected(true);
      setStatus(`Connected: ${chip}`);
    } catch (e) {
      setError(e instanceof Error ? e.message : "Connection failed");
      setConnected(false);
    }
  }

  /** Disconnect from ESP32 */
  async function handleDisconnect() {
    try {
      if (transportRef.current) {
        await transportRef.current.disconnect();
      }
    } catch {
      // ignore disconnect errors
    }
    transportRef.current = null;
    loaderRef.current = null;
    setConnected(false);
    setChipInfo("");
    setStatus("");
  }

  /** Fetch available GitHub releases */
  async function fetchReleases() {
    setLoadingReleases(true);
    setError("");
    try {
      const detected = detectGitHubRepo();
      const apiBase = detected
        ? `https://api.github.com/repos/${detected.owner}/${detected.repo}/releases`
        : releaseUrl
          ? releaseUrl
          : null;

      if (!apiBase) {
        setError("Could not determine GitHub repository. Enter a release URL.");
        return;
      }

      const resp = await fetch(apiBase);
      if (!resp.ok) throw new Error(`GitHub API: ${resp.status}`);
      const data = await resp.json();

      const tags = (data as { tag_name: string; html_url: string }[])
        .filter(
          (r: { tag_name: string }) =>
            r.tag_name && r.tag_name.startsWith("v")
        )
        .slice(0, 10)
        .map((r: { tag_name: string; html_url: string }) => ({
          tag: r.tag_name,
          url: r.html_url,
        }));

      setReleases(tags);
      if (tags.length > 0) setSelectedTag(tags[0].tag);
    } catch (e) {
      setError(e instanceof Error ? e.message : "Failed to fetch releases");
    } finally {
      setLoadingReleases(false);
    }
  }

  /** Download a binary from a GitHub release asset */
  async function downloadReleaseAsset(
    owner: string,
    repo: string,
    tag: string,
    filename: string
  ): Promise<string> {
    const url = `https://github.com/${owner}/${repo}/releases/download/${tag}/${filename}`;
    appendLog(`Downloading ${filename}...`);
    const resp = await fetch(url);
    if (!resp.ok) throw new Error(`Failed to download ${filename}: ${resp.status}`);
    const buf = await resp.arrayBuffer();
    appendLog(`Downloaded ${filename} (${buf.byteLength} bytes)`);
    return arrayBufferToBstr(buf);
  }

  /** Flash firmware to ESP32 */
  async function handleFlash() {
    if (!loaderRef.current) {
      setError("Not connected. Connect to ESP32 first.");
      return;
    }

    setError("");
    setFlashing(true);
    setFileProgress([]);

    try {
      let fileArray: { data: string; address: number }[];

      if (source === "files") {
        // Read local files
        const loaded: { data: string; address: number }[] = [];
        for (const slot of slots) {
          if (!slot.file) {
            if (slot.required) {
              setError(`Please select a file for: ${slot.label}`);
              setFlashing(false);
              return;
            }
            continue;
          }
          const data = await readFileAsBstr(slot.file);
          loaded.push({ data, address: slot.offset });
          appendLog(
            `Loaded ${slot.label}: ${slot.file.name} (${slot.file.size} bytes) @ 0x${slot.offset.toString(16)}`
          );
        }
        fileArray = loaded;
      } else {
        // Download from GitHub release
        const detected = detectGitHubRepo();
        if (!detected) {
          setError("Cannot determine GitHub repository.");
          setFlashing(false);
          return;
        }
        if (!selectedTag) {
          setError("Select a release version first.");
          setFlashing(false);
          return;
        }

        const { owner, repo } = detected;
        const bootloader = await downloadReleaseAsset(
          owner,
          repo,
          selectedTag,
          "bootloader-esp32s3.bin"
        );
        const partTable = await downloadReleaseAsset(
          owner,
          repo,
          selectedTag,
          "partition-table-esp32s3.bin"
        );
        const firmware = await downloadReleaseAsset(
          owner,
          repo,
          selectedTag,
          "firmware-esp32s3.bin"
        );

        fileArray = [
          { data: bootloader, address: OFFSETS.bootloader },
          { data: partTable, address: OFFSETS.partitionTable },
          { data: firmware, address: OFFSETS.firmware },
        ];
      }

      setFileProgress(fileArray.map(() => 0));

      setStatus("Erasing and writing flash...");

      const flashOptions: FlashOptions = {
        fileArray,
        flashSize: "8MB",
        flashMode: "dio",
        flashFreq: "80m",
        eraseAll: false,
        compress: true,
        reportProgress: (fileIndex: number, written: number, total: number) => {
          const pct = Math.round((written / total) * 100);
          setFileProgress((prev) => {
            const next = [...prev];
            next[fileIndex] = pct;
            return next;
          });
        },
      };

      await loaderRef.current.writeFlash(flashOptions);

      setStatus("Flash complete! Resetting device...");
      appendLog("Flash complete. Hard resetting via RTS pin...");
      await loaderRef.current.after("hard_reset", true);

      setStatus("Done! Device has been flashed and reset.");
    } catch (e) {
      setError(e instanceof Error ? e.message : "Flash failed");
    } finally {
      setFlashing(false);
    }
  }

  const gitHubDetected = detectGitHubRepo();
  const allFilesSelected = slots.every((s) => !s.required || s.file);
  const canFlash =
    connected &&
    !flashing &&
    (source === "release" ? !!selectedTag : allFilesSelected);

  return (
    <div style={{ padding: "2rem", maxWidth: "600px", margin: "0 auto" }}>
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: "1rem",
          marginBottom: "1.5rem",
        }}
      >
        <button
          onClick={() => nav("/")}
          style={{
            background: "none",
            border: "none",
            color: "#94a3b8",
            cursor: "pointer",
            fontSize: "1.2rem",
            padding: "0.25rem",
          }}
        >
          &larr;
        </button>
        <h2 style={{ margin: 0 }}>Flash Firmware</h2>
      </div>

      <p style={{ color: "#94a3b8", marginBottom: "1.5rem" }}>
        Flash firmware to your ESP32-S3 via USB. Put the device in download mode
        by holding BOOT while pressing RESET, then connect via USB.
      </p>

      {/* Step 1: Connect */}
      <div
        style={{
          marginBottom: "1.5rem",
          padding: "1rem",
          background: "#1e293b",
          borderRadius: "8px",
        }}
      >
        <h3 style={{ marginTop: 0, fontSize: "1rem" }}>1. Connect</h3>
        {!connected ? (
          <button
            onClick={handleConnect}
            style={{
              padding: "0.5rem 1rem",
              background: "#3b82f6",
              color: "white",
              border: "none",
              borderRadius: "6px",
              cursor: "pointer",
            }}
          >
            Connect to ESP32
          </button>
        ) : (
          <div
            style={{
              display: "flex",
              alignItems: "center",
              gap: "0.75rem",
            }}
          >
            <span style={{ color: "#22c55e" }}>Connected: {chipInfo}</span>
            <button
              onClick={handleDisconnect}
              disabled={flashing}
              style={{
                padding: "0.25rem 0.75rem",
                background: "#374151",
                color: "#94a3b8",
                border: "1px solid #4b5563",
                borderRadius: "6px",
                cursor: flashing ? "not-allowed" : "pointer",
                fontSize: "0.85rem",
              }}
            >
              Disconnect
            </button>
          </div>
        )}
      </div>

      {/* Step 2: Source */}
      <div
        style={{
          marginBottom: "1.5rem",
          padding: "1rem",
          background: "#1e293b",
          borderRadius: "8px",
        }}
      >
        <h3 style={{ marginTop: 0, fontSize: "1rem" }}>2. Firmware Source</h3>
        <div style={{ display: "flex", gap: "0.5rem", marginBottom: "1rem" }}>
          <button
            onClick={() => setSource("files")}
            style={{
              padding: "0.4rem 0.8rem",
              background: source === "files" ? "#3b82f6" : "#374151",
              color: "white",
              border: "none",
              borderRadius: "6px",
              cursor: "pointer",
            }}
          >
            Local Files
          </button>
          {gitHubDetected && (
            <button
              onClick={() => {
                setSource("release");
                if (!releases) fetchReleases();
              }}
              style={{
                padding: "0.4rem 0.8rem",
                background: source === "release" ? "#3b82f6" : "#374151",
                color: "white",
                border: "none",
                borderRadius: "6px",
                cursor: "pointer",
              }}
            >
              GitHub Release
            </button>
          )}
        </div>

        {source === "files" && (
          <div style={{ display: "flex", flexDirection: "column", gap: "0.75rem" }}>
            {slots.map((slot, i) => (
              <div key={i}>
                <label
                  style={{
                    display: "block",
                    color: "#94a3b8",
                    marginBottom: "0.25rem",
                    fontSize: "0.85rem",
                  }}
                >
                  {slot.label}
                  <span style={{ color: "#64748b" }}>
                    {" "}
                    (0x{slot.offset.toString(16)})
                  </span>
                </label>
                <input
                  type="file"
                  accept=".bin"
                  onChange={(e) => {
                    const files = (e.target as HTMLInputElement).files;
                    handleFileSelect(i, files?.[0] ?? null);
                  }}
                  style={{
                    display: "block",
                    color: "#e2e8f0",
                    fontSize: "0.85rem",
                  }}
                />
              </div>
            ))}
          </div>
        )}

        {source === "release" && (
          <div>
            {loadingReleases && (
              <p style={{ color: "#94a3b8" }}>Loading releases...</p>
            )}
            {releases && releases.length === 0 && (
              <p style={{ color: "#f59e0b" }}>
                No releases found. Build and tag a release first.
              </p>
            )}
            {releases && releases.length > 0 && (
              <div>
                <label
                  style={{
                    color: "#94a3b8",
                    fontSize: "0.85rem",
                    display: "block",
                    marginBottom: "0.25rem",
                  }}
                >
                  Version
                </label>
                <select
                  value={selectedTag}
                  onChange={(e) =>
                    setSelectedTag((e.target as HTMLSelectElement).value)
                  }
                  style={{
                    padding: "0.4rem",
                    background: "#0f172a",
                    color: "#e2e8f0",
                    border: "1px solid #334155",
                    borderRadius: "6px",
                    fontSize: "0.9rem",
                    width: "100%",
                  }}
                >
                  {releases.map((r) => (
                    <option key={r.tag} value={r.tag}>
                      {r.tag}
                    </option>
                  ))}
                </select>
                <p
                  style={{
                    color: "#64748b",
                    fontSize: "0.8rem",
                    marginTop: "0.5rem",
                  }}
                >
                  Will download bootloader, partition table, and firmware from
                  this release.
                </p>
              </div>
            )}
            {!releases && !loadingReleases && (
              <div>
                {!gitHubDetected && (
                  <div style={{ marginBottom: "0.5rem" }}>
                    <label
                      style={{
                        color: "#94a3b8",
                        fontSize: "0.85rem",
                        display: "block",
                        marginBottom: "0.25rem",
                      }}
                    >
                      GitHub Releases API URL
                    </label>
                    <input
                      type="text"
                      placeholder="https://api.github.com/repos/owner/repo/releases"
                      value={releaseUrl}
                      onInput={(e) =>
                        setReleaseUrl((e.target as HTMLInputElement).value)
                      }
                      style={{
                        width: "100%",
                        padding: "0.4rem",
                        background: "#0f172a",
                        color: "#e2e8f0",
                        border: "1px solid #334155",
                        borderRadius: "6px",
                        fontSize: "0.9rem",
                        boxSizing: "border-box",
                      }}
                    />
                  </div>
                )}
                <button
                  onClick={fetchReleases}
                  style={{
                    padding: "0.4rem 0.8rem",
                    background: "#374151",
                    color: "white",
                    border: "none",
                    borderRadius: "6px",
                    cursor: "pointer",
                  }}
                >
                  Load Releases
                </button>
              </div>
            )}
          </div>
        )}
      </div>

      {/* Step 3: Flash */}
      <div
        style={{
          marginBottom: "1.5rem",
          padding: "1rem",
          background: "#1e293b",
          borderRadius: "8px",
        }}
      >
        <h3 style={{ marginTop: 0, fontSize: "1rem" }}>3. Flash</h3>
        <button
          onClick={handleFlash}
          disabled={!canFlash}
          style={{
            padding: "0.6rem 1.5rem",
            background: canFlash ? "#f97316" : "#374151",
            color: "white",
            border: "none",
            borderRadius: "6px",
            fontSize: "1rem",
            cursor: canFlash ? "pointer" : "not-allowed",
            opacity: canFlash ? 1 : 0.5,
          }}
        >
          {flashing ? "Flashing..." : "Flash Firmware"}
        </button>

        {/* Progress bars */}
        {fileProgress.length > 0 && (
          <div style={{ marginTop: "1rem" }}>
            {fileProgress.map((pct, i) => {
              const label =
                source === "files"
                  ? slots[i]?.label
                  : ["Bootloader", "Partition Table", "Firmware"][i];
              return (
                <div key={i} style={{ marginBottom: "0.5rem" }}>
                  <div
                    style={{
                      display: "flex",
                      justifyContent: "space-between",
                      fontSize: "0.8rem",
                      color: "#94a3b8",
                      marginBottom: "0.2rem",
                    }}
                  >
                    <span>{label}</span>
                    <span>{pct}%</span>
                  </div>
                  <div
                    style={{
                      height: "6px",
                      background: "#0f172a",
                      borderRadius: "3px",
                      overflow: "hidden",
                    }}
                  >
                    <div
                      style={{
                        height: "100%",
                        width: `${pct}%`,
                        background: pct === 100 ? "#22c55e" : "#f97316",
                        borderRadius: "3px",
                        transition: "width 0.2s",
                      }}
                    />
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* Status & error */}
      {status && (
        <p style={{ color: "#94a3b8", marginBottom: "0.5rem" }}>{status}</p>
      )}
      {error && (
        <p style={{ color: "#ef4444", marginBottom: "0.5rem" }}>{error}</p>
      )}

      {/* Console log */}
      {logLines.length > 0 && (
        <div
          ref={logRef}
          style={{
            background: "#0f172a",
            border: "1px solid #334155",
            borderRadius: "8px",
            padding: "0.75rem",
            maxHeight: "200px",
            overflow: "auto",
            fontFamily: "monospace",
            fontSize: "0.75rem",
            color: "#94a3b8",
            lineHeight: "1.4",
            whiteSpace: "pre-wrap",
            wordBreak: "break-all",
          }}
        >
          {logLines.map((line, i) => (
            <div key={i}>{line}</div>
          ))}
        </div>
      )}
    </div>
  );
}
