import { test } from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import createLamp from "../public/generated/lamp.js";
import { decodeFrame, FrameParser } from "../src/protocol.mjs";
const wasmBinary = readFileSync(
  new URL("../public/generated/lamp.wasm", import.meta.url),
);
const module = await createLamp({ wasmBinary });
const bytes = () => {
  const pointer = module._lamp_frame();
  return module.HEAPU8.slice(pointer, pointer + module._lamp_frame_size());
};
test("simulator capture contains the actual ADC inputs and shared detector output", () => {
  module._lamp_reset(42);
  module._lamp_select(2);
  const batches = [];
  const parser = new FrameParser(batch => batches.push(batch));
  for (let i = 0; i < 12; i++) {
    module._lamp_advance(1, 1023, 0);
    const pointer = module._lamp_audio_data();
    parser.push(module.HEAPU8.slice(pointer, pointer + module._lamp_audio_size()));
  }
  assert.equal(batches.length, 10);
  assert.deepEqual(batches[0].samples.map(sample => sample.raw), [0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023]);
  assert.ok(batches.some(batch => batch.samples.some(sample => sample.onset > 0)));
  assert.equal(batches[9].startMicros, 90000);
  module._lamp_select(0);
  module._lamp_advance(1, 1023, 0);
  assert.equal(module._lamp_audio_size(), 0);
  module._lamp_reset(42);
  assert.equal(module._lamp_audio_size(), 0);
});
test("WebAssembly matches native C++ byte-for-byte across all programs, seeds, audio and button transitions", () => {
  const expected = execFileSync(
    "uv",
    ["run", "--locked", "python", "scripts/native_trace.py"],
    { encoding: "utf8", maxBuffer: 2 * 1024 * 1024 },
  )
    .trim()
    .split("\n");
  let cursor = 0;
  for (const seed of [42, 1337])
    for (let program = 0; program < 8; program++) {
      module._lamp_reset(seed);
      module._lamp_select(program);
      module._lamp_brightness(177);
      for (let frame = 0; frame < 360; frame++) {
        module._lamp_advance(
          1,
          (frame * 73) % 1024,
          +(frame >= 100 && frame < 110),
        );
        assert.equal(
          Buffer.from(bytes()).toString("hex"),
          expected[cursor++],
          `seed=${seed}, program=${program}, frame=${frame}`,
        );
      }
    }
  module._lamp_reset(1337);
  module._lamp_select(2);
  let detected = false;
  for (let frame = 0; frame < 3000; frame++) {
    const time = Math.floor(frame * 8333 / 1000);
    const period = frame < 1440 ? 500 : 428;
    module._lamp_advance(1, frame < 2640 && time % period < 40 ? 700 : 0, 0);
    const packet = bytes();
    assert.equal(Buffer.from(packet).toString("hex"), expected[cursor++], `beat frame=${frame}`);
    detected ||= decodeFrame(packet).bpm > 0;
  }
  assert.ok(detected);
  assert.equal(cursor, expected.length);
});
test("deterministic restart, real audio envelope and independent module instances", async () => {
  const other = await createLamp({ wasmBinary });
  module._lamp_reset(1337);
  other._lamp_reset(1337);
  module._lamp_select(5);
  other._lamp_select(5);
  module._lamp_advance(120, 0, 0);
  other._lamp_advance(120, 0, 0);
  const pointer = other._lamp_frame();
  assert.deepEqual(bytes(), other.HEAPU8.slice(pointer, pointer + other._lamp_frame_size()));
  const original = bytes();
  module._lamp_reset(1337);
  module._lamp_select(5);
  module._lamp_advance(120, 0, 0);
  assert.deepEqual(bytes(), original);
  module._lamp_reset(42);
  module._lamp_select(2);
  module._lamp_advance(12, 1023, 0);
  assert.ok(decodeFrame(bytes()).audio > 240);
  assert.equal(decodeFrame(bytes()).rgb[0], 255);
  module._lamp_advance(240, 0, 0);
  assert.equal(decodeFrame(bytes()).audio, 0);
  assert.equal(module._lamp_select(255), 0);
  assert.equal(module.UTF8ToString(module._lamp_program_name(7)), "Rainbow");
});

test("the shared simulator locks to musical pulses, flashes on its clock, and releases silence", () => {
  module._lamp_reset(42);
  module._lamp_select(2);
  for (let frame = 0; frame < 1680; frame++) {
    const time = frame * module._lamp_frame_interval_us() / 1000;
    module._lamp_advance(1, Math.round(700 * Math.exp(-(time % 500) / 35)), 0);
  }
  const locked = decodeFrame(bytes());
  assert.ok(Math.abs(locked.bpm - 120) <= 2, `BPM ${locked.bpm}`);
  assert.ok(locked.confidence >= 55);
  const levels = [];
  for (let i = 0; i < 120; i++) {
    module._lamp_advance(1, 0, 0);
    levels.push(decodeFrame(bytes()).rgb[0]);
  }
  assert.ok(levels.some(level => level > 100));
  assert.ok(levels.some(level => level === 0));
  module._lamp_advance(480, 0, 0);
  assert.equal(decodeFrame(bytes()).bpm, 0);
});
