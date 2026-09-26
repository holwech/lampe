import { test } from "node:test";
import assert from "node:assert/strict";
import { AudioHistory } from "../src/audio-history.mjs";
const batch = (startMicros, sequence) => ({ startMicros, sequence,
  samples: Array.from({ length: 10 }, (_, i) => ({ offset: i * 1000, raw: 512 + i, onset: 5, beat: i === 0 })),
});
test("capture history uses device time across rollover and leaves holes for lost packets", () => {
  const history = new AudioHistory();
  history.append(batch(0xfffff000, 255));
  history.append(batch((0xfffff000 + 10000) >>> 0, 0));
  assert.equal(history.points[10].time, 10);
  assert.equal(history.points[10].gap, false);
  history.append(batch((0xfffff000 + 30000) >>> 0, 2));
  assert.equal(history.points[20].time, 30);
  assert.equal(history.points[20].gap, true);
  assert.match(history.csv(), /^time_ms,adc,onset,beat,gap\n0.000,512,5,1,1\n/);
  history.append(batch(100, 0)); // Device restarted.
  assert.equal(history.points.length, 10);
  assert.equal(history.points[0].time, 0);
});
test("capture storage and export stay bounded to five seconds", () => {
  const history = new AudioHistory();
  for (let i = 0; i < 1000; i++) history.append(batch(i * 10000, i & 255));
  assert.equal(history.points.length, 5001);
  assert.equal(history.points.at(-1).time - history.points[0].time, 5000);
  assert.equal(history.csv().trim().split("\n").length, 5002);
  history.clear();
  assert.equal(history.points.length, 0);
});
