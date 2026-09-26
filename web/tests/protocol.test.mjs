import { test } from "node:test";
import assert from "node:assert/strict";
import {
  crc8,
  decodeFrame,
  FrameParser,
  PACKET_SIZE,
  selectProgramCommand,
  captureCommand, decodeAudio,
} from "../src/protocol.mjs";
function packet(sequence = 9, version = 4) {
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
  assert.equal(frame.rawAudio, true);
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
  unknown[2] = 5;
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
  assert.equal(decodeFrame(packet(8, 3)).programControl, true);
  assert.equal(decodeFrame(packet(8, 3)).rawAudio, false);
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

function audioPacket(sequence = 0) {
  const bytes = new Uint8Array(60);
  bytes.set([76, 65, 1, 10, sequence, 0, 0xf0, 255, 255]);
  const view = new DataView(bytes.buffer);
  for (let i = 0; i < 10; i++) {
    view.setUint16(9 + i * 5, i * 1100, true);
    view.setUint16(11 + i * 5, i === 0 ? 0x8200 : i === 9 ? 1023 : 512, true);
    bytes[13 + i * 5] = 42;
  }
  bytes[59] = crc8(bytes.subarray(0, 59));
  return bytes;
}
test("raw ADC packets retain timestamps, full 10-bit samples, onsets and beat phase", () => {
  const decoded = decodeAudio(audioPacket());
  assert.equal(decoded.startMicros, 0xfffff000);
  assert.deepEqual(decoded.samples[0], { offset: 0, raw: 512, onset: 42, beat: true });
  assert.deepEqual(decoded.samples[9], { offset: 9900, raw: 1023, onset: 42, beat: false });
  for (const field of [3, 9, 12, 59]) {
    const invalid = audioPacket();
    invalid[field] = 255;
    if (field !== 59) invalid[59] = crc8(invalid.subarray(0, 59));
    assert.equal(decodeAudio(invalid), null);
  }
  const invalid = audioPacket();
  invalid[14] = invalid[15] = 0;
  invalid[59] = crc8(invalid.subarray(0, 59));
  assert.equal(decodeAudio(invalid), null);
  for (const enabled of [true, false]) {
    const command = captureCommand(enabled);
    assert.deepEqual([...command.slice(0, 5)], [76, 67, 1, 2, +enabled]);
    assert.equal(command[5], crc8(command.slice(0, 5)));
  }
});
test("interleaved microphone and lamp packets survive every split and recover after corrupt capture", () => {
  const raw = audioPacket(), good = packet(), corrupt = audioPacket();
  corrupt[22] ^= 32;
  const stream = Uint8Array.from([...raw, ...good, ...corrupt, ...raw, ...packet(8, 1)]);
  for (let split = 0; split <= stream.length; split++) {
    const audio = [], parser = new FrameParser(batch => audio.push(batch));
    const frames = [...parser.push(stream.slice(0, split)), ...parser.push(stream.slice(split))];
    assert.deepEqual(frames, [decodeFrame(good), decodeFrame(packet(8, 1))]);
    assert.deepEqual(audio, [decodeAudio(raw), decodeAudio(raw)]);
    assert.equal(parser.pending.length, 0);
  }
});

test("program commands match the firmware vector and reject non-byte inputs", () => {
  assert.deepEqual([...selectProgramCommand(7)], [76, 67, 1, 1, 7, 148]);
  for (const value of [-1, 256, 2.5, NaN])
    assert.throws(() => selectProgramCommand(value), RangeError);
});
