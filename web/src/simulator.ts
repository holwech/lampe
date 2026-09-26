import { decodeFrame, FrameParser, PACKET_SIZE, type decodeAudio } from "./protocol.mjs";
export type Frame = NonNullable<ReturnType<typeof decodeFrame>>;
export type AudioBatch = NonNullable<ReturnType<typeof decodeAudio>>;
interface LampModule {
  HEAPU8: Uint8Array;
  UTF8ToString(pointer: number): string;
  _lamp_reset(seed: number): void;
  _lamp_select(program: number): number;
  _lamp_advance(frames: number, audio: number, button: number): void;
  _lamp_frame(): number;
  _lamp_frame_size(): number;
  _lamp_audio_data(): number;
  _lamp_audio_size(): number;
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
    this.module._lamp_reset(1337);
  }
  static async load() {
    // Load the generated Emscripten module as a static asset in both dev and
    // preview. A root-relative dynamic import gets Vite's ?import transform,
    // which rejects JS served from public/.
    const url = new URL("/generated/lamp.js", window.location.origin).href;
    const { default: createLamp } = await import(/* @vite-ignore */ url);
    return new Simulator(await createLamp());
  }
  select(program: number) {
    this.module._lamp_select(program);
  }
  advance(frames: number, audio: number, button = false) {
    this.module._lamp_advance(frames, audio, +button);
  }
  audio(onAudio: (batch: AudioBatch) => void) {
    const pointer = this.module._lamp_audio_data();
    new FrameParser(onAudio).push(this.module.HEAPU8.slice(pointer, pointer + this.module._lamp_audio_size()));
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
