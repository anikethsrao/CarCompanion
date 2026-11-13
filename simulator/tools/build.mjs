#!/usr/bin/env node
// Compiles an Arduino sketch (.ino) into a WebAssembly "firmware" the
// simulator can run. Uses a Clang toolchain that itself runs on Node
// (@yowasp/clang), so nothing besides Node and `npm install` is needed.
//
//   node tools/build.mjs path/to/Sketch.ino [-o out.wasm]
import { readFile, writeFile, readdir } from 'node:fs/promises';
import { basename, dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { runClang } from '@yowasp/clang';
import binaryen from 'binaryen';

const here = dirname(fileURLToPath(import.meta.url));
const coreDir = resolve(here, '..', 'core');

// Replace comments and string contents with spaces, keeping offsets and newlines.
function blankComments(src) {
  let out = '', i = 0;
  while (i < src.length) {
    const c = src[i], n = src[i + 1];
    if (c === '/' && n === '/') { while (i < src.length && src[i] !== '\n') { out += ' '; i++; } }
    else if (c === '/' && n === '*') {
      out += '  '; i += 2;
      while (i < src.length && !(src[i] === '*' && src[i + 1] === '/')) { out += src[i] === '\n' ? '\n' : ' '; i++; }
      if (i < src.length) { out += '  '; i += 2; }
    } else if (c === '"' || c === "'") {
      const q = c; out += c; i++;
      while (i < src.length && src[i] !== q) { if (src[i] === '\\') { out += ' '; i++; } out += src[i] === '\n' ? '\n' : ' '; i++; }
      if (i < src.length) { out += q; i++; }
    } else { out += c; i++; }
  }
  return out;
}

// Arduino generates prototypes for sketch functions so they can be called
// before they are defined. Do the same.
function findFunctions(clean) {
  const depthAt = new Int32Array(clean.length + 1);
  let d = 0;
  for (let i = 0; i < clean.length; i++) { depthAt[i] = d; if (clean[i] === '{') d++; else if (clean[i] === '}') d--; }
  const re = /(^|[;}\n])\s*((?:(?:static|inline|const|unsigned|signed|long|short|struct)\s+)*[A-Za-z_][\w:<>]*(?:\s*[*&]+\s*|\s+))([A-Za-z_]\w*)\s*\(([^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*(?:const\s*)?\{/g;
  const fns = [];
  let m;
  while ((m = re.exec(clean))) {
    const start = m.index + m[1].length;
    if (depthAt[start] !== 0) continue;
    const ret = m[2].trim(), name = m[3];
    if (/^(if|while|for|switch|return|else|do|sizeof)$/.test(name) || /^(return|else|new|delete)$/.test(ret)) continue;
    const params = m[4].replace(/\s+/g, ' ').trim();
    const line = clean.slice(0, start).split('\n').length;
    fns.push({ ret, name, params, line, start });
    re.lastIndex = m.index + m[0].length;
  }
  return fns;
}

export function preprocess(src, fileName = 'sketch.ino') {
  const clean = blankComments(src);
  const fns = findFunctions(clean);
  const bitmaps = [];
  const bre = /\b(?:const\s+)?(?:unsigned\s+char|uint8_t|byte|char)\s+(?:const\s+)?([A-Za-z_]\w*)\s*\[[^\]]*\]\s*PROGMEM\s*=/g;
  let m;
  while ((m = bre.exec(clean))) bitmaps.push(m[1]);
  const protos = fns
    .filter(f => !['setup', 'loop'].includes(f.name))
    .map(f => `${f.ret} ${f.name}(${f.params.replace(/=\s*[^,]+/g, '')});`);
  // Insert prototypes right before the first function definition.
  const insertAt = fns.length ? src.lastIndexOf('\n', fns[0].start) + 1 : src.length;
  const firstLine = src.slice(0, insertAt).split('\n').length;
  const f = JSON.stringify(fileName);
  let out = `#include <Arduino.h>\n#line 1 ${f}\n` + src.slice(0, insertAt) +
    `\n${protos.join('\n')}\n#line ${firstLine} ${f}\n` + src.slice(insertAt);
  // Procedural faces (CarFace.h) have no bitmaps to name, so ask the face
  // object for its mood instead.
  const faceObj = /(?:carface::)?CarFace\s*<[^>]*>\s+([A-Za-z_]\w*)\s*[({]/.exec(clean);
  out += faceObj
    ? `\n\nstatic char sim_label_buf[64];\nextern "C" __attribute__((export_name("sim_label"))) const char *sim_label() {\n` +
      `  const char *m = carface::MOOD_NAMES[${faceObj[1]}.mood()];\n` +
      `  float lx = ${faceObj[1]}.params()[carface::LOOK_X];\n` +
      `  snprintf(sim_label_buf, sizeof sim_label_buf, "%s%s", m, lx > 8 ? ", leaning right" : lx < -8 ? ", leaning left" : "");\n` +
      `  return sim_label_buf;\n}\n`
    : '';
  out += `\n\nextern "C" void sim_register_bitmap_c(const char *, const uint8_t *, int);\n` +
    `extern "C" void sim_register_sketch_bitmaps() {\n` +
    bitmaps.map(b => `  sim_register_bitmap_c(${JSON.stringify(b)}, (const uint8_t *)${b}, (int)sizeof(${b}));`).join('\n') +
    `\n}\n`;
  return { code: out, functions: fns.map(f => f.name), bitmaps };
}

export async function buildSketch(inoPath, { optimize = true, log = console.error } = {}) {
  const src = await readFile(inoPath, 'utf8');
  const name = basename(inoPath);
  const { code, bitmaps } = preprocess(src, name);
  const files = { 'sketch.cpp': code };
  const core = {};
  async function addDir(dir, prefix) {
    for (const e of await readdir(dir, { withFileTypes: true })) {
      if (e.isDirectory()) { core[e.name] = {}; await addDir(join(dir, e.name), e.name); }
      else (prefix ? core[prefix] : core)[e.name] = await readFile(join(dir, e.name));
    }
  }
  await addDir(coreDir, '');
  files.core = core;
  // Other .h/.cpp files next to the sketch are part of the sketch, like in the Arduino IDE.
  const sketchDir = dirname(resolve(inoPath));
  const extra = [];
  for (const e of await readdir(sketchDir)) {
    if (/\.(h|hpp)$/.test(e)) files[e] = await readFile(join(sketchDir, e));
    if (/\.(cpp|c)$/.test(e)) { files[e] = await readFile(join(sketchDir, e)); extra.push(e); }
  }
  let output = '';
  const args = ['clang++', '--target=wasm32-wasip1', '-mexec-model=reactor', '-std=gnu++17',
    optimize ? '-O2' : '-O0', '-fno-exceptions', '-fno-rtti', '-Wno-write-strings', '-Wno-unknown-attributes',
    '-Icore', '-I.', 'sketch.cpp', 'core/sim_runtime.cpp', 'core/Adafruit_GFX.cpp', ...extra,
    '-Wl,-z,stack-size=65536', '-o', 'firmware.wasm'];
  let result;
  try {
    result = await runClang(args, files, {
      stdout: (b) => { if (b) output += new TextDecoder().decode(b); },
      stderr: (b) => { if (b) output += new TextDecoder().decode(b); },
    });
  } catch (e) {
    const err = new Error('Compile failed\n' + output.replaceAll('sketch.cpp', name));
    err.compilerOutput = output;
    throw err;
  }
  if (output.trim()) log(output);
  const wasm = result['firmware.wasm'];
  // Asyncify lets delay() pause the sketch and hand control back to the page.
  const mod = binaryen.readBinary(wasm);
  binaryen.setOptimizeLevel(optimize ? 2 : 0);
  binaryen.setPassArgument('asyncify-imports', 'env.sim_delay,env.sim_display');
  mod.runPasses(['asyncify']);
  if (optimize) mod.optimize();
  const out = mod.emitBinary();
  mod.dispose();
  return { wasm: out, bitmaps, warnings: output };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  const ino = args.find(a => !a.startsWith('-'));
  const oi = args.indexOf('-o');
  if (!ino) { console.error('usage: node tools/build.mjs Sketch.ino [-o firmware.wasm]'); process.exit(1); }
  const outPath = oi >= 0 ? args[oi + 1] : basename(ino).replace(/\.ino$/, '') + '.wasm';
  try {
    const { wasm, bitmaps } = await buildSketch(ino);
    await writeFile(outPath, wasm);
    console.error(`built ${outPath} (${(wasm.length / 1024).toFixed(0)} KB, ${bitmaps.length} bitmaps)`);
  } catch (e) { console.error(e.message); process.exit(1); }
}
