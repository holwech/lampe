import { FrameParser } from "./protocol.mjs";
import type { Frame } from "./simulator";
interface Port {
  readable: ReadableStream<Uint8Array> | null;
  open(options: {
    baudRate: number;
    bufferSize: number;
    flowControl: string;
  }): Promise<void>;
  close(): Promise<void>;
  setSignals(signals: {
    dataTerminalReady: boolean;
    requestToSend: boolean;
  }): Promise<void>;
}
interface SerialAPI {
  requestPort(): Promise<Port>;
}
const serialAPI = () =>
  (navigator as Navigator & { serial?: SerialAPI }).serial;
export const serialSupported = () => !!serialAPI();

export class LampConnection {
  private reader: ReadableStreamDefaultReader<Uint8Array> | null = null;
  private port: Port | null = null;
  private reading: Promise<void> | null = null;
  private closing = false;
  parser = new FrameParser();
  constructor(
    private onFrame: (frame: Frame) => void,
    private onEnd: (message: string) => void,
  ) {}
  async connect() {
    const api = serialAPI();
    if (!api)
      throw new Error(
        "Live mode needs Web Serial. Open this local dashboard in Chrome or Edge.",
      );
    const port = await api.requestPort();
    await port.open({
      baudRate: 115200,
      bufferSize: 4096,
      flowControl: "none",
    });
    this.port = port;
    this.closing = false;
    this.parser = new FrameParser();
    try {
      // Some adapters reset the board on open before these signals can be cleared.
      await port.setSignals({ dataTerminalReady: false, requestToSend: false });
      if (!port.readable) throw new Error("This port has no readable stream.");
      this.reader = port.readable.getReader();
      this.reading = this.read();
    } catch (error) {
      await port.close().catch(() => {});
      this.port = null;
      throw error;
    }
  }
  private async read() {
    let message = "Lamp disconnected. The last received frame is held.";
    try {
      while (this.reader && !this.closing) {
        const { value, done } = await this.reader.read();
        if (done) break;
        if (value)
          for (const frame of this.parser.push(value)) this.onFrame(frame);
      }
    } catch (error) {
      message = `Serial connection ended: ${error instanceof Error ? error.message : String(error)}`;
    } finally {
      this.reader?.releaseLock();
      this.reader = null;
      if (!this.closing) {
        await this.port?.close().catch(() => {});
        this.port = null;
        this.onEnd(message);
      }
    }
  }
  async disconnect() {
    this.closing = true;
    await this.reader?.cancel().catch(() => {});
    await this.reading;
    await this.port?.close().catch(() => {});
    this.port = null;
    this.reading = null;
  }
}
