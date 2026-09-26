import { test } from "node:test";
import assert from "node:assert/strict";
import {
  crc8,
  decodeFrame,
  FrameParser,
  PACKET_SIZE,
  selectProgramCommand,
} from "../src/protocol.mjs";
function packet(sequence = 9, version = 3) {
  const bytes = new Uint8Array(version === 1 ? 61 : PACKET_SIZE);
  bytes.set([76, 77, version, 16, 2, 100, 255, sequence, 0x78, 0x56, 0x34, 0x12]);
  for (let i = 12; i < 60; i++) bytes[i] = (i * 37) % 256;
  if (version >= 2) bytes.set([120, 90], 60);
  bytes[bytes.length - 1] = crc8(bytes.subarray(0, -1));
  return bytes;
}
test("CRC-8 check value and little endian fields", () => {
  assert.equal(crc8(new TextEncoder().encode("123456789")), 0xf4);
  const frame = decodeFrame(packet());
  assert.equal(frame.time, 0x12345678);
  assert.equal(frame.sequence, 9);
  assert.equal(frame.rgb.length, 48);
  assert.equal(frame.audio, 255);
  assert.equal(frame.bpm, 120);
  assert.equal(frame.confidence, 90);
  assert.equal(frame.programControl, true);
});
test("every possible serial packet split and multi-packet reads", () => {
  const bytes = packet();
  for (let split = 1; split < bytes.length; split++) {
    const parser = new FrameParser();
    assert.deepEqual(parser.push(bytes.slice(0, split)), []);
    assert.deepEqual(parser.push(bytes.slice(split)), [decodeFrame(bytes)]);
    assert.equal(parser.pending.length, 0);
  }
  assert.equal(
    new FrameParser().push(Uint8Array.from([...bytes, ...bytes, ...bytes]))
      .length,
    3,
  );
});
test("resynchronizes after noise, dropped bytes, bad checksums and unknown versions", () => {
  const good = packet();
  const corrupt = packet();
  corrupt[28] ^= 1;
  const unknown = packet();
  unknown[2] = 4;
  unknown[62] = crc8(unknown.subarray(0, 62));
  const stream = Uint8Array.from([
    0,
    255,
    76,
    ...corrupt,
    ...unknown,
    ...good.slice(0, 23),
    ...good,
  ]);
  const parser = new FrameParser();
  const frames = [];
  for (const byte of stream) frames.push(...parser.push(Uint8Array.of(byte)));
  assert.deepEqual(frames, [decodeFrame(good)]);
  assert.ok(parser.rejected >= 3);
  assert.ok(parser.pending.length < PACKET_SIZE);
  parser.push(new Uint8Array(1_000_000));
  assert.ok(parser.pending.length < PACKET_SIZE);
});

test("legacy firmware and mixed protocol versions remain readable at every split", () => {
  const legacy = packet(8, 1), current = packet(9);
  assert.equal(decodeFrame(legacy).bpm, null);
  assert.equal(decodeFrame(legacy).confidence, null);
  const previous = packet(8, 2);
  assert.equal(decodeFrame(previous).bpm, 120);
  assert.equal(decodeFrame(previous).programControl, false);
  assert.equal(decodeFrame(legacy).programControl, false);
  const stream = Uint8Array.from([...legacy, ...current, ...previous]);
  for (let split = 1; split < stream.length; split++) {
    const parser = new FrameParser();
    assert.deepEqual([...parser.push(stream.slice(0, split)), ...parser.push(stream.slice(split))],
      [decodeFrame(legacy), decodeFrame(current), decodeFrame(previous)]);
  }
});

test("program commands match the firmware vector and reject non-byte inputs", () => {
  assert.deepEqual([...selectProgramCommand(7)], [76, 67, 1, 1, 7, 148]);
  for (const value of [-1, 256, 2.5, NaN])
    assert.throws(() => selectProgramCommand(value), RangeError);
});
