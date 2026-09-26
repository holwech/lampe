import { decodeFrame, PACKET_SIZE } from "./protocol.mjs";
export type Frame = NonNullable<ReturnType<typeof decodeFrame>>;
interface LampModule {
  HEAPU8: Uint8Array;
  UTF8ToString(pointer: number): string;
  _lamp_reset(seed: number): void;
  _lamp_select(program: number): number;
  _lamp_advance(frames: number, audio: number, button: number): void;
  _lamp_frame(): number;
  _lamp_frame_size(): number;
  _lamp_program_count(): number;
  _lamp_frame_interval_us(): number;
  _lamp_program_name(program: number): number;
  _lamp_program_uses_audio(program: number): number;
}
export class Simulator {
  readonly programs: { name: string; audio: boolean }[];
  readonly interval: number;
  constructor(private module: LampModule) {
    if (module._lamp_frame_size() !== PACKET_SIZE)
      throw new Error("Simulator protocol mismatch. Rebuild the dashboard.");
    this.programs = Array.from(
      { length: module._lamp_program_count() },
      (_, i) => ({
        name: module.UTF8ToString(module._lamp_program_name(i)),
        audio: !!module._lamp_program_uses_audio(i),
      }),
    );
    this.interval = module._lamp_frame_interval_us() / 1000;
    this.reset(1337);
  }
  static async load() {
    // Load the generated Emscripten module as a static asset in both dev and
    // preview. A root-relative dynamic import gets Vite's ?import transform,
    // which rejects JS served from public/.
    const url = new URL("/generated/lamp.js", window.location.origin).href;
    const { default: createLamp } = await import(/* @vite-ignore */ url);
    return new Simulator(await createLamp());
  }
  reset(seed: number) {
    this.module._lamp_reset(seed);
  }
  select(program: number) {
    this.module._lamp_select(program);
  }
  advance(frames: number, audio: number, button = false) {
    this.module._lamp_advance(frames, audio, +button);
  }
  frame(): Frame {
    const pointer = this.module._lamp_frame();
    const frame = decodeFrame(
      this.module.HEAPU8.slice(pointer, pointer + PACKET_SIZE),
    );
    if (!frame) throw new Error("Invalid frame from the simulator.");
    return frame;
  }
}
