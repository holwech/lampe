// Five seconds of received samples. Device timestamps, not USB arrival times,
// preserve real spacing and let the charts leave holes for dropped batches.
export class AudioHistory {
  points = [];
  lastStart = null;
  lastSequence = null;
  time = 0;
  clear() {
    this.points = [];
    this.lastStart = this.lastSequence = null;
    this.time = 0;
  }
  /** @param {NonNullable<ReturnType<import('./protocol.mjs').decodeAudio>>} batch */
  append(batch) {
    const delta = this.lastStart === null ? 0 : (batch.startMicros - this.lastStart) >>> 0;
    if (delta > 5_000_000) this.clear(); // Reset/reconnect or long interruption.
    else this.time += delta / 1000;
    let missing = this.lastSequence !== null && batch.sequence !== ((this.lastSequence + 1) & 255);
    this.lastStart = batch.startMicros;
    this.lastSequence = batch.sequence;
    for (const sample of batch.samples) {
      const time = this.time + sample.offset / 1000;
      const previous = this.points.at(-1);
      const gap = !previous || missing || time - previous.time > 2.5;
      this.points.push({ time, raw: sample.raw, onset: sample.onset, beat: sample.beat, gap });
      missing = false;
    }
    const cutoff = this.points.at(-1).time - 5000;
    const first = this.points.findIndex(point => point.time >= cutoff);
    if (first > 0) this.points.splice(0, first);
    if (this.points.length > 5100) this.points.splice(0, this.points.length - 5100);
  }
  csv() {
    const start = this.points[0]?.time ?? 0;
    return "time_ms,adc,onset,beat,gap\n" + this.points.map(point =>
      `${(point.time - start).toFixed(3)},${point.raw},${point.onset},${+point.beat},${+point.gap}`,
    ).join("\n") + "\n";
  }
}
