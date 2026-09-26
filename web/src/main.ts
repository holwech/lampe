import "./style.css";
import { LampScene } from "./scene";
import { Simulator, type Frame } from "./simulator";
import { LampConnection, serialSupported } from "./serial";
import { AudioView } from "./audio-view";

const $ = <T extends HTMLElement = HTMLElement>(id: string) =>
  document.getElementById(id) as T;
const button = (id: string) => $<HTMLButtonElement>(id);
const input = (id: string) => $<HTMLInputElement>(id);
const select = (id: string) => $<HTMLSelectElement>(id);
const audioView = new AudioView();
const swatches = [
  "linear-gradient(90deg,#f29a5b 0 45%,#7d9dba 45%)",
  "linear-gradient(90deg,#bb819e,#bdc981,#7d9dba)",
  "linear-gradient(90deg,#d57968,#ebbb91)",
  "linear-gradient(90deg,#c77c73 0 50%,#839caf 50%)",
  "linear-gradient(90deg,#b292ba,#a2b975)",
  "linear-gradient(90deg,#cd885a,#e8c476)",
  "linear-gradient(90deg,#72a690,#a6bca1)",
  "linear-gradient(90deg,#d99979,#d5c482,#a4b48c,#8eabc3,#c18bab)",
];
let simulator: Simulator;
let scene: LampScene | null = null;
let frame: Frame;
let mode: "sim" | "live" = "sim";
let busy = false,
  connected = false,
  hasLiveFrame = false;
let selectedPixel = 0,
  accumulator = 0;
let animationFrame: number | null = null;
let pendingProgram: number | null = null;
let lastTick = performance.now(),
  lastReceived = 0,
  connectedAt = 0,
  lastUi = 0;
const pixelButtons: HTMLButtonElement[] = [];
const programButtons: HTMLButtonElement[] = [];

function notice(message: string, error = false) {
  $("notice").textContent = message;
  $("notice").hidden = !message;
  $("notice").classList.toggle("error", error);
}
function stageMessage(message: string) {
  $("stage-message").textContent = message;
  $("stage-message").hidden = !message;
}
function showFrame(value: Frame) {
  if (frame?.program !== value.program) audioView.clear();
  frame = value;
  scene?.update(value);
}
const connection = new LampConnection(
  (value) => {
    if (mode !== "live") return;
    const now = performance.now();
    lastReceived = now;
    if (!hasLiveFrame) {
      hasLiveFrame = true;
      notice("");
    }
    showFrame(value);
    stageMessage("");
    updateControls();
  },
  (message) => {
    connected = false;
    busy = false;
    if (mode === "live") {
      notice(message, true);
      stageMessage("Disconnected · last frame held");
      updateControls();
    }
  },
  (batch) => {
    if (mode === "live" && !document.hidden && !$("audio-section").hidden) audioView.append(batch);
  },
  (message) => audioView.failed(message),
);

function updateControls() {
  const live = mode === "live";
  button("sim-mode").setAttribute("aria-pressed", String(!live));
  button("live-mode").setAttribute("aria-pressed", String(live));
  button("sim-mode").disabled = button("live-mode").disabled = busy;
  $("live-info").hidden = !live;
  button("connect").disabled = busy || !serialSupported();
  button("connect").textContent = busy
    ? "Connecting…"
    : connected
      ? "Disconnect"
      : "Connect lamp";
  programButtons.forEach((b, i) => {
    b.disabled = live && (busy || !connected || !hasLiveFrame || !frame.programControl ||
      performance.now() - lastReceived > 500 || pendingProgram !== null);
    b.setAttribute("aria-busy", String(live && pendingProgram === i));
    b.setAttribute(
      "aria-pressed",
      String((!live || hasLiveFrame) && i === frame.program),
    );
  });
  const audio =
    (!live || hasLiveFrame) && !!simulator.programs[frame.program]?.audio;
  select("audio-mode").disabled = input("audio-level").disabled =
    live || !audio;
  $("audio-section").hidden = !audio;
  $("microphone-panel").hidden = !audio;
  connection.setCapture(live && connected && hasLiveFrame && audio && frame.rawAudio && !document.hidden);
  $("audio-source").hidden = $("audio-input").hidden = live;
  $("tempo-input").hidden = live || select("audio-mode").value !== "pulse";
  $("source-label").hidden = !live;
  $("source-label").dataset.connected = String(connected && hasLiveFrame);
  $("connection-status").textContent = busy
    ? "Connecting"
    : connected
      ? hasLiveFrame
        ? "Connected"
        : "Waiting"
      : "Offline";
  $("programs").title = live && hasLiveFrame && !frame.programControl
    ? "Update firmware to change programs from the app." : "";
}

async function setMode(value: "sim" | "live") {
  if (busy || mode === value) return;
  busy = true;
  updateControls();
  await connection.disconnect();
  mode = value;
  connected = false;
  busy = false;
  hasLiveFrame = false;
  lastReceived = 0;
  accumulator = 0;
  audioView.clear();
  if (mode === "sim") {
    showFrame(simulator.frame());
    stageMessage("");
    notice("");
  } else {
    showFrame({
      program: 0,
      brightness: 0,
      audio: 0,
      bpm: null,
      confidence: null,
      programControl: false,
      rawAudio: false,
      sequence: 0,
      time: 0,
      rgb: new Uint8Array(48),
    });
    stageMessage("");
    notice(
      serialSupported() ? "" : "Open in Chrome or Edge to connect the lamp.",
      !serialSupported(),
    );
  }
  updateControls();
}

function advance() {
  const time = simulator.frame().time;
  const audioMode = select("audio-mode").value;
  const pulse =
    audioMode === "pulse"
      ? Math.exp(-((time % (60000 / Number(input("audio-tempo").value))) / 35))
      : 1;
  const level =
    audioMode === "silence"
      ? 0
      : Math.round((Number(input("audio-level").value) / 100) * 1023 * pulse);
  simulator.advance(1, level);
  simulator.audio(batch => audioView.append(batch));
}
function inspect(index: number) {
  selectedPixel = index;
  scene?.select(index);
  pixelButtons.forEach((b, i) =>
    b.setAttribute("aria-pressed", String(i === index)),
  );
}
function updateReadouts(now: number) {
  if (!$("microphone-panel").hidden) audioView.render(mode === "live", connected, frame.rawAudio);
  const tempo = input("audio-tempo").value;
  $("audio-tempo-value").textContent = `${tempo} BPM`;
  $("tempo-input").hidden = mode === "live" || select("audio-mode").value !== "pulse";
  const stale = mode === "live" && (!connected || !hasLiveFrame || now - lastReceived > 500);
  $("beat-value").textContent = stale ? "—" : frame.bpm === null ? "Update firmware"
    : frame.bpm ? `${frame.bpm} BPM` : "Listening…";
  $("beat-value").title = frame.bpm && !stale ? `Tempo confidence: ${frame.confidence}%` : "";
  $("audio-level-value").textContent = `${input("audio-level").value}%`;
  $("audio-value").textContent = `${Math.round((frame.audio / 255) * 100)}%`;
  $("audio-meter").style.width = `${(frame.audio / 255) * 100}%`;
  $("audio-meter").parentElement!.setAttribute(
    "aria-valuenow",
    String(frame.audio),
  );
  for (let i = 0; i < 16; i++) {
    const rgb = Array.from(frame.rgb.slice(i * 3, i * 3 + 3));
    pixelButtons[i].firstElementChild!.setAttribute(
      "style",
      `background:rgb(${rgb.join(",")})`,
    );
    pixelButtons[i].title = `Pixel ${i}: RGB ${rgb.join(", ")}`;
  }
  const rgb = Array.from(
    frame.rgb.slice(selectedPixel * 3, selectedPixel * 3 + 3),
  );
  $("pixel-name").textContent =
    `Pixel ${String(selectedPixel).padStart(2, "0")}`;
  $("pixel-rgb").textContent = rgb
    .map((c, i) => `${"RGB"[i]} ${String(c).padStart(3, "0")}`)
    .join("  ");
  $("pixel-hex").textContent = `#${rgb
    .map((c) => c.toString(16).padStart(2, "0"))
    .join("")
    .toUpperCase()}`;
  if (mode === "live") {
    const age = now - lastReceived;
    if (connected && hasLiveFrame && age > 500) {
      programButtons.forEach((b) => b.disabled = true);
      stageMessage("Signal paused · last frame held");
      $("source-label").dataset.connected = "false";
      $("connection-status").textContent = "Paused";
    }
    if (connected && !busy && !hasLiveFrame && now - connectedAt > 5000) {
      stageMessage("Waiting for lamp data");
      notice(
        "No lamp data. Check the 5 V supply, selected port, and firmware.",
        true,
      );
    }
  }
}

function scheduleAnimation() {
  if (!document.hidden && animationFrame === null)
    animationFrame = requestAnimationFrame(animate);
}

function animate(now: number) {
  animationFrame = null;
  if (document.hidden) return;
  // Cap long stalls; visibility changes reset the clock before resuming.
  const elapsed = Math.min(now - lastTick, 100);
  lastTick = now;
  if (mode === "sim") {
    accumulator += elapsed;
    while (accumulator >= simulator.interval) {
      advance();
      accumulator -= simulator.interval;
    }
    showFrame(simulator.frame());
  }
  if (now - lastUi > 50) {
    updateReadouts(now);
    lastUi = now;
  }
  scene?.render();
  scheduleAnimation();
}

async function start() {
  try {
    scene = new LampScene($<HTMLCanvasElement>("lamp-canvas"));
  } catch (error) {
    notice(
      `The 3D view needs WebGL. LED inspection is still available. ${String(error)}`,
      true,
    );
  }
  simulator = await Simulator.load();
  showFrame(simulator.frame());
  simulator.programs.forEach((program, i) => {
    const b = document.createElement("button");
    b.className = "program";
    b.dataset.program = String(i);
    const top = document.createElement("span");
    top.className = "program-top";
    const icon = document.createElement("span");
    icon.className = "program-icon";
    icon.style.setProperty("--swatch", swatches[i % swatches.length]);
    icon.setAttribute("aria-hidden", "true");
    const check = document.createElement("span");
    check.className = "program-check";
    check.textContent = "✓";
    check.setAttribute("aria-hidden", "true");
    top.append(icon, check);
    const label = document.createElement("span");
    label.textContent = program.name;
    b.append(top, label);
    b.addEventListener("click", async () => {
      if (mode === "live") {
        if (!connected || !hasLiveFrame || !frame.programControl || pendingProgram !== null ||
          performance.now() - lastReceived > 500 || frame.program === i) return;
        pendingProgram = i;
        notice("");
        updateControls();
        try {
          await connection.selectProgram(i);
        } catch (error) {
          if (mode === "live" && !(error instanceof DOMException && error.name === "AbortError"))
            notice(error instanceof Error ? error.message : String(error), true);
        } finally {
          pendingProgram = null;
          updateControls();
        }
        return;
      }
      simulator.select(i);
      showFrame(simulator.frame());
      accumulator = 0;
      updateControls();
    });
    programButtons.push(b);
    $("programs").append(b);
  });
  for (let i = 0; i < 16; i++) {
    const b = document.createElement("button");
    b.className = "pixel";
    b.setAttribute("aria-label", `Inspect pixel ${i}`);
    b.innerHTML = `<span></span>`;
    b.addEventListener("click", () => inspect(i));
    pixelButtons.push(b);
    $("led-strip").append(b);
  }
  inspect(0);
  button("sim-mode").addEventListener("click", () => void setMode("sim"));
  button("live-mode").addEventListener("click", () => void setMode("live"));
  button("connect").addEventListener("click", async () => {
    if (busy) return;
    if (connected) {
      busy = true;
      updateControls();
      await connection.disconnect();
      connected = false;
      busy = false;
      stageMessage("Disconnected · last frame held");
      updateControls();
      return;
    }
    busy = true;
    hasLiveFrame = false;
    audioView.clear();
    updateControls();
    notice("");
    try {
      // Set this before starting the reader: an immediate EOF may clear it.
      connected = true;
      await connection.connect();
      connectedAt = performance.now();
      if (connected && !hasLiveFrame) stageMessage("Waiting for lamp data…");
    } catch (error) {
      connected = false;
      if (error instanceof DOMException && error.name === "NotFoundError")
        notice("No port selected. Connect whenever you’re ready.");
      else notice(error instanceof Error ? error.message : String(error), true);
      stageMessage("");
    } finally {
      busy = false;
      updateControls();
    }
  });
  button("diffuser").addEventListener("click", () => {
    const visible = button("diffuser").getAttribute("aria-pressed") !== "true";
    button("diffuser").setAttribute("aria-pressed", String(visible));
    button("diffuser").setAttribute(
      "aria-label",
      `Diffuser ${visible ? "on" : "off"}`,
    );
    scene?.diffuser(visible);
  });
  $("lamp-canvas").addEventListener("pixel-select", (event) =>
    inspect((event as CustomEvent<number>).detail),
  );
  document.addEventListener("visibilitychange", () => {
    accumulator = 0;
    lastTick = performance.now();
    updateControls();
    if (animationFrame !== null) {
      cancelAnimationFrame(animationFrame);
      animationFrame = null;
    }
    scheduleAnimation();
  });
  if (scene) notice("");
  updateControls();
  updateReadouts(performance.now());
  lastTick = performance.now();
  scheduleAnimation();
}
start().catch((error) => {
  notice(
    `Could not load the lamp engine. Run “npm run build:simulator” and reload. ${error instanceof Error ? error.message : error}`,
    true,
  );
  stageMessage("Lamp engine unavailable");
});
