import { FrameParser, selectProgramCommand } from "./protocol.mjs";
import type { Frame } from "./simulator";
interface Port {
  readable: ReadableStream<Uint8Array> | null;
  writable: WritableStream<Uint8Array> | null;
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
  private writer: WritableStreamDefaultWriter<Uint8Array> | null = null;
  private port: Port | null = null;
  private reading: Promise<void> | null = null;
  private closing = false;
  private programControl = false;
  private pending: {
    program: number;
    resolve: () => void;
    reject: (error: Error) => void;
    timer: ReturnType<typeof setTimeout>;
  } | null = null;
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
    this.programControl = false;
    this.parser = new FrameParser();
    try {
      // Some adapters reset the board on open before these signals can be cleared.
      await port.setSignals({ dataTerminalReady: false, requestToSend: false });
      if (!port.readable) throw new Error("This port has no readable stream.");
      this.writer = port.writable?.getWriter() ?? null;
      this.reader = port.readable.getReader();
      this.reading = this.read();
    } catch (error) {
      this.writer?.releaseLock();
      this.writer = null;
      await port.close().catch(() => {});
      this.port = null;
      throw error;
    }
  }
  selectProgram(program: number): Promise<void> {
    const writer = this.writer;
    if (!writer || this.closing || !this.programControl)
      return Promise.reject(new Error("Connect a lamp with program-control firmware first."));
    if (this.pending)
      return Promise.reject(new Error("A program change is already pending."));
    const bytes = selectProgramCommand(program);
    return new Promise((resolve, reject) => {
      const request = {
        program, resolve, reject,
        timer: setTimeout(() => {
          if (this.pending === request)
            this.finishCommand(new Error("The lamp did not confirm the program change. Try again."));
        }, 2000),
      };
      this.pending = request;
      void writer.write(bytes).catch((error) => {
        if (this.pending === request)
          this.finishCommand(error instanceof Error ? error : new Error(String(error)));
      });
    });
  }
  private finishCommand(error?: Error) {
    const pending = this.pending;
    if (!pending) return;
    this.pending = null;
    clearTimeout(pending.timer);
    if (error) pending.reject(error);
    else pending.resolve();
  }
  private async releaseWriter() {
    const writer = this.writer;
    this.writer = null;
    if (writer) {
      await writer.abort().catch(() => {});
      writer.releaseLock();
    }
  }
  private async read() {
    let message = "Lamp disconnected. The last received frame is held.";
    try {
      while (this.reader && !this.closing) {
        const { value, done } = await this.reader.read();
        if (done) break;
        if (value)
          for (const frame of this.parser.push(value)) {
            this.programControl = frame.programControl;
            if (frame.programControl && this.pending?.program === frame.program) this.finishCommand();
            this.onFrame(frame);
          }
      }
    } catch (error) {
      message = `Serial connection ended: ${error instanceof Error ? error.message : String(error)}`;
    } finally {
      this.reader?.releaseLock();
      this.reader = null;
      if (!this.closing) {
        this.programControl = false;
        this.finishCommand(new Error(message));
        await this.releaseWriter();
        await this.port?.close().catch(() => {});
        this.port = null;
        this.onEnd(message);
      }
    }
  }
  async disconnect() {
    this.closing = true;
    this.programControl = false;
    this.finishCommand(new DOMException("Program change cancelled.", "AbortError"));
    await this.releaseWriter();
    await this.reader?.cancel().catch(() => {});
    await this.reading;
    await this.port?.close().catch(() => {});
    this.port = null;
    this.reading = null;
  }
}
