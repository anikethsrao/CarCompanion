#!/usr/bin/env node
// Runs drives against a sketch without a browser and prints what the screen
// showed and when. Handy for checking a change before trying it in the car.
//
//   node tools/run.mjs Sketch.ino                 # every built-in drive
//   node tools/run.mjs Sketch.ino --drive lane    # one drive
//   node tools/run.mjs firmware.wasm --serial     # also print Serial output
import { readFile } from 'node:fs/promises';
import { Firmware, frameLabel } from '../web/engine.js';
import { SCENARIOS, scenarioAt, scenarioDuration, Sensor } from '../web/world.js';

export async function runDrive(wasm, sc, { serial = false } = {}) {
  const sensor = new Sensor(sc.settings, 7);
  let speed = 0, lastT = 0;
  const motionAt = (t) => {
    const m = scenarioAt(sc, t);
    if (t > lastT) { speed = Math.max(0, speed + m.lon * 9.81 * (t - lastT)); lastT = t; }
    return { ...m, speed };
  };
  const segments = [];   // { t0, t1, face }
  const log = [];
  const fw = new Firmware(wasm, {
    sensor: (t) => sensor.read(t, motionAt(t)),
    onFrame: (f) => {
      const face = frameLabel(f);
      const last = segments[segments.length - 1];
      if (last && last.face === face) return;
      if (last) last.t1 = f.t;
      segments.push({ t0: f.t, t1: null, face, drive: scenarioAt(sc, f.t).label });
    },
    onSerial: ({ t, line }) => { if (serial || !/Acceleration:|Sensitivity|forward g:/.test(line)) log.push({ t, line }); },
    onEvent: (e) => { if (e.name === 'crash') log.push({ t: e.t, line: `CRASH: ${e.message}` }); },
  });
  await fw.start();
  const end = scenarioDuration(sc) + 2;
  fw.runUntil(end * 1e6);
  if (segments.length) segments[segments.length - 1].t1 = end;
  return { segments, log, frames: fw.frames, loops: fw.loops, baud: fw.baud };
}

export function formatRun(sc, r) {
  const lines = [`== ${sc.name} (${sc.id}) ==`];
  // Fold quick animation frames (< 0.2 s each) into one row.
  const rows = [];
  for (const s of r.segments) {
    const dur = s.t1 - s.t0, quick = dur < 0.2;
    const prev = rows[rows.length - 1];
    if (quick && prev && prev.quick) { prev.t1 = s.t1; if (prev.faces[prev.faces.length - 1] !== s.face) prev.faces.push(s.face); continue; }
    rows.push({ t0: s.t0, t1: s.t1, faces: [s.face], drive: s.drive, quick });
  }
  for (const row of rows) {
    let face = row.faces.join(' > ');
    if (face.length > 60) face = face.slice(0, 57) + '...';
    lines.push(`  ${row.t0.toFixed(2).padStart(6)}s  ${(row.t1 - row.t0).toFixed(2).padStart(5)}s  ${face.padEnd(30)}  [${row.drive}]`);
  }
  return lines.join('\n');
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const args = process.argv.slice(2);
  const file = args.find(a => !a.startsWith('-') && args[args.indexOf(a) - 1] !== '--drive');
  if (!file) { console.error('usage: node tools/run.mjs Sketch.ino|firmware.wasm [--drive id] [--serial]'); process.exit(1); }
  let wasm;
  if (file.endsWith('.ino')) {
    const { buildSketch } = await import('./build.mjs');
    wasm = (await buildSketch(file, { log: () => {} })).wasm;
  } else wasm = await readFile(file);
  const di = args.indexOf('--drive');
  const list = di >= 0 ? SCENARIOS.filter(s => s.id === args[di + 1]) : SCENARIOS;
  if (!list.length) { console.error('drives: ' + SCENARIOS.map(s => s.id).join(', ')); process.exit(1); }
  for (const sc of list) {
    const r = await runDrive(wasm, sc, { serial: args.includes('--serial') });
    console.log(formatRun(sc, r));
    if (args.includes('--serial')) for (const l of r.log) console.log(`    ${l.t.toFixed(2)}s  ${l.line}`);
  }
}
