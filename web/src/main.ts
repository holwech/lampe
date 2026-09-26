import "./style.css";
import { LampScene } from "./scene";
import { Simulator, type Frame } from "./simulator";
import { LampConnection, serialSupported } from "./serial";

const $ = <T extends HTMLElement = HTMLElement>(id: string) =>
  document.getElementById(id) as T;
const button = (id: string) => $<HTMLButtonElement>(id);
const input = (id: string) => $<HTMLInputElement>(id);
const select = (id: string) => $<HTMLSelectElement>(id);
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
let playing = true,
  busy = false,
  connected = false,
  hasLiveFrame = false;
let selectedPixel = 0,
  simulatedProgram = 0,
  accumulator = 0;
let lastTick = performance.now(),
  lastReceived = 0,
  connectedAt = 0,
  lastUi = 0;
let receivedTimes: number[] = [],
  missed = 0,
  previousLive: Frame | null = null;
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
  frame = value;
  scene?.update(value);
}
const connection = new LampConnection(
  (value) => {
    if (mode !== "live") return;
    const now = performance.now();
    if (previousLive && value.time >= previousLive.time)
      missed += Math.max(
        0,
        ((value.sequence - previousLive.sequence + 256) % 256) - 1,
      );
    previousLive = value;
    lastReceived = now;
    receivedTimes.push(now);
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
  for (const id of ["play", "restart", "step", "apply-seed"])
    button(id).disabled = live;
  input("seed").disabled = live;
  select("speed").disabled = live;
  button("play").innerHTML =
    `<svg class="icon" aria-hidden="true"><use href="#icon-${playing ? "pause" : "play"}" /></svg>`;
  button("play").title = playing ? "Pause simulation" : "Play simulation";
  button("play").setAttribute(
    "aria-label",
    playing ? "Pause simulation" : "Play simulation",
  );
  programButtons.forEach((b, i) => {
    b.disabled = live;
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
  $("audio-source").hidden = $("audio-input").hidden = live;
  $("seed-controls").hidden = live;
  $("transport-actions").hidden = live;
  $("live-help").hidden = !live;
  $("clock-label").textContent = live ? "Lamp uptime" : "Simulation time";
  $("clock").title = live ? "Lamp uptime" : "Simulation time";
  $("source-label").hidden = !live;
  $("source-label").dataset.connected = String(connected && hasLiveFrame);
  $("connection-status").textContent = busy
    ? "Connecting"
    : connected
      ? hasLiveFrame
        ? "Connected"
        : "Waiting"
      : "Offline";
  $("programs").title = live ? "Change programs with the lamp’s button" : "";
  $("engine-status").textContent = live
    ? connected
      ? "Connected"
      : "Disconnected"
    : playing
      ? "Running"
      : "Paused";
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
  receivedTimes = [];
  missed = 0;
  previousLive = null;
  accumulator = 0;
  if (mode === "sim") {
    showFrame(simulator.frame());
    stageMessage("");
    notice("");
  } else {
    showFrame({
      program: 0,
      brightness: 0,
      audio: 0,
      sequence: 0,
      time: 0,
      rgb: new Uint8Array(48),
    });
    stageMessage("");
    notice(
      serialSupported()
        ? ""
        : "Open in Chrome or Edge to connect the lamp.",
      !serialSupported(),
    );
  }
  updateControls();
}

function restart() {
  const seed = Number(input("seed").value);
  if (!Number.isInteger(seed) || seed < 0 || seed > 65535) {
    notice("Choose a whole-number seed between 0 and 65535.", true);
    return;
  }
  simulator.reset(seed);
  simulator.select(simulatedProgram);
  accumulator = 0;
  showFrame(simulator.frame());
  notice("");
  updateControls();
}
function advance() {
  const time = simulator.frame().time;
  const audioMode = select("audio-mode").value;
  const pulse =
    audioMode === "pulse"
      ? Math.pow(Math.max(0, Math.sin((time / 1000) * Math.PI * 2)), 3)
      : 1;
  const level =
    audioMode === "silence"
      ? 0
      : Math.round((Number(input("audio-level").value) / 100) * 1023 * pulse);
  simulator.advance(1, level);
}
function inspect(index: number) {
  selectedPixel = index;
  scene?.select(index);
  pixelButtons.forEach((b, i) =>
    b.setAttribute("aria-pressed", String(i === index)),
  );
}
function updateReadouts(now: number) {
  const milliseconds = frame.time % 1000;
  const seconds = Math.floor(frame.time / 1000) % 60;
  const minutes = Math.floor(frame.time / 60000);
  $("clock").textContent =
    `${String(minutes).padStart(2, "0")}:${String(seconds).padStart(2, "0")}.${String(milliseconds).padStart(3, "0")}`;
  $("clock").dataset.time = String(frame.time);
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
    receivedTimes = receivedTimes.filter((t) => now - t < 1000);
    const age = now - lastReceived;
    $("stream-status").textContent = hasLiveFrame
      ? `${receivedTimes.length} frames/s · last ${Math.round(age)} ms ago · ${missed} missed · ${connection.parser.rejected} invalid`
      : "Waiting for a valid lamp frame · 115200 baud";
    if (connected && hasLiveFrame && age > 500) {
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
  } else $("stream-status").textContent = "120 Hz";
}

function animate(now: number) {
  // Discard time while hidden or after a long stall; never catch up a giant backlog.
  const elapsed = Math.min(now - lastTick, 100);
  lastTick = now;
  if (mode === "sim" && playing && !document.hidden) {
    accumulator += elapsed * Number(select("speed").value);
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
  requestAnimationFrame(animate);
}

document.addEventListener("keydown", (event) => {
  if (event.key === "Escape") $("help").removeAttribute("open");
});
document.addEventListener("click", (event) => {
  if (!$("help").contains(event.target as Node))
    $("help").removeAttribute("open");
});

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
    b.addEventListener("click", () => {
      simulatedProgram = i;
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
    previousLive = null;
    receivedTimes = [];
    missed = 0;
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
  button("play").addEventListener("click", () => {
    playing = !playing;
    accumulator = 0;
    updateControls();
  });
  button("restart").addEventListener("click", restart);
  button("apply-seed").addEventListener("click", restart);
  input("seed").addEventListener("keydown", (event) => {
    if (event.key === "Enter") restart();
  });
  button("step").addEventListener("click", () => {
    playing = false;
    accumulator = 0;
    advance();
    showFrame(simulator.frame());
    updateControls();
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
  button("reset-view").addEventListener("click", () => scene?.resetView());
  $("lamp-canvas").addEventListener("pixel-select", (event) =>
    inspect((event as CustomEvent<number>).detail),
  );
  document.addEventListener("visibilitychange", () => {
    accumulator = 0;
    lastTick = performance.now();
  });
  if (scene) notice("");
  updateControls();
  updateReadouts(performance.now());
  requestAnimationFrame(animate);
}
start().catch((error) => {
  notice(
    `Could not load the lamp engine. Run “npm run build:simulator” and reload. ${error instanceof Error ? error.message : error}`,
    true,
  );
  stageMessage("Lamp engine unavailable");
});
