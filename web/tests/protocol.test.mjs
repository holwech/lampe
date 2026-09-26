import { test } from "node:test";
import assert from "node:assert/strict";
import {
  crc8,
  decodeFrame,
  FrameParser,
  PACKET_SIZE,
} from "../src/protocol.mjs";
function packet(sequence = 9) {
  const bytes = new Uint8Array(PACKET_SIZE);
  bytes.set([76, 77, 1, 16, 2, 100, 255, sequence, 0x78, 0x56, 0x34, 0x12]);
  for (let i = 12; i < 60; i++) bytes[i] = (i * 37) % 256;
  bytes[60] = crc8(bytes.subarray(0, 60));
  return bytes;
}
test("CRC-8 check value and little endian fields", () => {
  assert.equal(crc8(new TextEncoder().encode("123456789")), 0xf4);
  const frame = decodeFrame(packet());
  assert.equal(frame.time, 0x12345678);
  assert.equal(frame.sequence, 9);
  assert.equal(frame.rgb.length, 48);
  assert.equal(frame.audio, 255);
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
  unknown[2] = 2;
  unknown[60] = crc8(unknown.subarray(0, 60));
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
