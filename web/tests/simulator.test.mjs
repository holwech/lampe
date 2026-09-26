import { test } from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import createLamp from "../public/generated/lamp.js";
import { decodeFrame } from "../src/protocol.mjs";
const wasmBinary = readFileSync(
  new URL("../public/generated/lamp.wasm", import.meta.url),
);
const module = await createLamp({ wasmBinary });
const bytes = () => {
  const pointer = module._lamp_frame();
  return module.HEAPU8.slice(pointer, pointer + module._lamp_frame_size());
};
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
  assert.deepEqual(bytes(), other.HEAPU8.slice(pointer, pointer + 61));
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
