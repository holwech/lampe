export const LED_COUNT = 16;
export const PACKET_SIZE = 63;

const packetSize = (version) => version === 1 ? 61 : version === 2 ? 63 : 0;

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
    bpm: packet[2] === 2 ? packet[60] : null,
    confidence: packet[2] === 2 ? packet[61] : null,
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
      if (data[offset] !== 76 || data[offset + 1] !== 77) {
        offset++;
        continue;
      }
      const size = packetSize(data[offset + 2]);
      if (!size) {
        this.rejected++;
        offset++;
        continue;
      }
      if (offset + size > data.length) break;
      const frame = decodeFrame(data.subarray(offset, offset + size));
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
