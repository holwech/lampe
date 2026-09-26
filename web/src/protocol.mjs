export const LED_COUNT = 16;
export const PACKET_SIZE = 63;

const packetSize = (version) => version === 1 ? 61 : version >= 2 && version <= 4 ? 63 : 0;
export const AUDIO_PACKET_SIZE = 60;

/** @param {Uint8Array} bytes */
export function crc8(bytes) {
  let crc = 0;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++)
      crc = ((crc << 1) ^ (crc & 0x80 ? 0x07 : 0)) & 255;
  }
  return crc;
}

/** @param {number} program */
export function selectProgramCommand(program) {
  if (!Number.isInteger(program) || program < 0 || program > 255)
    throw new RangeError("Program must be an unsigned byte.");
  const bytes = Uint8Array.of(76, 67, 1, 1, program, 0);
  bytes[5] = crc8(bytes.subarray(0, 5));
  return bytes;
}

/** @param {boolean} enabled */
export function captureCommand(enabled) {
  const bytes = Uint8Array.of(76, 67, 1, 2, enabled ? 1 : 0, 0);
  bytes[5] = crc8(bytes.subarray(0, 5));
  return bytes;
}

/** @param {Uint8Array} packet */
export function decodeAudio(packet) {
  if (packet.length !== AUDIO_PACKET_SIZE || packet[0] !== 76 || packet[1] !== 65 ||
      packet[2] !== 1 || packet[3] !== 10 || crc8(packet.subarray(0, -1)) !== packet[59]) return null;
  const view = new DataView(packet.buffer, packet.byteOffset, packet.byteLength);
  const samples = [];
  for (let i = 0; i < 10; i++) {
    const at = 9 + i * 5, value = view.getUint16(at + 2, true), offset = view.getUint16(at, true);
    if (value & 0x7c00 || (i === 0 ? offset !== 0 : offset <= samples[i - 1].offset)) return null;
    samples.push({ offset, raw: value & 1023, onset: packet[at + 4], beat: !!(value & 0x8000) });
  }
  return { sequence: packet[4], startMicros: view.getUint32(5, true), samples };
}

/** @param {Uint8Array} packet */
export function decodeFrame(packet) {
  if (
    packet.length !== packetSize(packet[2]) ||
    packet[0] !== 76 ||
    packet[1] !== 77 ||
    !packetSize(packet[2]) ||
    packet[3] !== LED_COUNT ||
    crc8(packet.subarray(0, -1)) !== packet[packet.length - 1]
  )
    return null;
  return {
    program: packet[4],
    brightness: packet[5],
    audio: packet[6],
    bpm: packet[2] >= 2 ? packet[60] : null,
    confidence: packet[2] >= 2 ? packet[61] : null,
    programControl: packet[2] >= 3,
    rawAudio: packet[2] >= 4,
    sequence: packet[7],
    time: new DataView(
      packet.buffer,
      packet.byteOffset,
      packet.byteLength,
    ).getUint32(8, true),
    rgb: packet.slice(12, 60),
  };
}

// Keeps at most one partial packet, including when attached mid-stream or after corruption.
export class FrameParser {
  /** @param {(batch: NonNullable<ReturnType<typeof decodeAudio>>) => void} onAudio */
  constructor(onAudio = () => {}) { this.onAudio = onAudio; }
  pending = new Uint8Array(0);
  rejected = 0;
  /** @param {Uint8Array} chunk */
  push(chunk) {
    const data = new Uint8Array(this.pending.length + chunk.length);
    data.set(this.pending);
    data.set(chunk, this.pending.length);
    const frames = [];
    let offset = 0;
    while (offset + 3 <= data.length) {
      if (data[offset] !== 76 || (data[offset + 1] !== 77 && data[offset + 1] !== 65)) {
        offset++;
        continue;
      }
      const audio = data[offset + 1] === 65;
      const size = audio ? (data[offset + 2] === 1 ? AUDIO_PACKET_SIZE : 0) : packetSize(data[offset + 2]);
      if (!size) {
        this.rejected++;
        offset++;
        continue;
      }
      if (offset + size > data.length) break;
      const packet = data.subarray(offset, offset + size);
      const frame = audio ? null : decodeFrame(packet);
      const batch = audio ? decodeAudio(packet) : null;
      if (batch) {
        this.onAudio(batch);
        offset += size;
        continue;
      }
      if (frame) {
        frames.push(frame);
        offset += size;
      } else {
        this.rejected++;
        offset++;
      }
    }
    this.pending = data.slice(offset);
    return frames;
  }
}
