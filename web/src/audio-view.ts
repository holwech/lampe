import { AudioHistory } from "./audio-history.mjs";
import type { AudioBatch } from "./simulator";

const element = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;
export class AudioView {
  history = new AudioHistory();
  private receivedAt = 0;
  private error = "";
  constructor() {
    element<HTMLButtonElement>("save-audio").addEventListener("click", () => {
      const url = URL.createObjectURL(new Blob([this.history.csv()], { type: "text/csv" }));
      const link = document.createElement("a");
      link.href = url;
      link.download = `lampe-microphone-${new Date().toISOString().replaceAll(":", "-")}.csv`;
      link.click();
      setTimeout(() => URL.revokeObjectURL(url), 1000);
    });
  }
  clear() { this.history.clear(); this.receivedAt = 0; this.error = ""; }
  append(batch: AudioBatch) {
    this.history.append(batch);
    this.receivedAt = performance.now();
    this.error = "";
  }
  failed(message: string) { this.error = message; }
  render(live: boolean, connected: boolean, supported: boolean) {
    const points = this.history.points;
    const end = points.at(-1)?.time ?? 0;
    const fresh = !!points.length && performance.now() - this.receivedAt < 500;
    const status = !live ? "Simulated" : !connected ? "Disconnected" : !supported
      ? "Update firmware for microphone capture" : this.error ? "Capture unavailable"
        : fresh ? "Live" : points.length ? "Signal paused" : "Waiting for microphone…";
    const span = end - (points[0]?.time ?? end);
    const rate = span > 0 ? (points.length - 1) / span : 0;
    element("capture-status").textContent = `${status}${fresh ? ` · ${rate.toFixed(1)} kHz` : ""}`;
    element("capture-status").title = this.error || "Timestamped ADC samples; up to 1,000 per second.";
    element<HTMLButtonElement>("save-audio").disabled = !points.length;
    const recent = points.filter(point => point.time > end - 500);
    let min = 1023, max = 0, clipped = 0;
    for (const point of recent) {
      min = Math.min(min, point.raw); max = Math.max(max, point.raw);
      if (point.raw === 0 || point.raw === 1023) clipped++;
    }
    element("mic-range").textContent = recent.length ? `${min}–${max}` : "—";
    element("mic-clipping").hidden = !fresh || !clipped;
    element("mic-clipping").textContent = "Clipping";
    element("mic-clipping").title = `${clipped} displayed samples reached the ADC limit.`;
    this.plot("waveform", 500, 1023, "raw", "#7198a6", end);
    this.plot("rhythm", 5000, 255, "onset", "#8ba463", end);
  }
  private plot(id: string, duration: number, maximum: number, field: "raw" | "onset", color: string, end: number) {
    const canvas = element<HTMLCanvasElement>(id);
    const context = canvas.getContext("2d");
    if (!context || !canvas.clientWidth) return;
    const width = canvas.clientWidth, height = canvas.clientHeight;
    const ratio = Math.min(window.devicePixelRatio || 1, 2);
    if (canvas.width !== Math.round(width * ratio) || canvas.height !== Math.round(height * ratio)) {
      canvas.width = Math.round(width * ratio); canvas.height = Math.round(height * ratio);
    }
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.clearRect(0, 0, width, height);
    const left = 35, top = 10, bottom = height - 22, right = width - 10;
    const x = (time: number) => left + (time - end + duration) / duration * (right - left);
    const y = (value: number) => bottom - value / maximum * (bottom - top);
    context.font = "10px ui-monospace, monospace";
    context.textAlign = "right";
    for (const value of [0, Math.round(maximum / 2), maximum]) {
      context.strokeStyle = "#dce2d4"; context.lineWidth = 1;
      context.beginPath(); context.moveTo(left, y(value)); context.lineTo(right, y(value)); context.stroke();
      context.fillStyle = "#7a8673"; context.fillText(String(value), left - 6, y(value) + 3);
    }
    context.textAlign = "left";
    context.fillText(duration === 500 ? "−500 ms" : "−5 s", left, height - 3);
    context.textAlign = "right"; context.fillText("now", right, height - 3);
    const points = this.history.points.filter(point => point.time >= end - duration);
    if (field === "onset") {
      let beat = false;
      for (const point of points) {
        if (point.beat && !beat) {
          context.strokeStyle = "#c9806380";
          context.beginPath(); context.moveTo(x(point.time), top); context.lineTo(x(point.time), bottom); context.stroke();
        }
        beat = point.beat;
      }
    }
    context.strokeStyle = color; context.lineWidth = 1.5;
    context.beginPath();
    points.forEach((point, i) => {
      if (!i || point.gap) context.moveTo(x(point.time), y(point[field]));
      else context.lineTo(x(point.time), y(point[field]));
    });
    context.stroke();
    canvas.setAttribute("aria-label", field === "raw"
      ? `Microphone waveform, last 500 milliseconds, ADC scale 0 to 1023. ${element("mic-range").textContent}.`
      : "Sound attacks over five seconds, strength 0 to 255. Orange lines mark detected beat flashes.");
  }
}
