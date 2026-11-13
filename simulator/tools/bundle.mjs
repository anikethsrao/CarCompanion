#!/usr/bin/env node
// Builds one or more sketches and writes a single self-contained HTML page
// with the simulator and those firmware builds inside it.
//
//   node tools/bundle.mjs -o dist/simulator.html "V0.3=../firmware/CarCompanion/CarCompanion.ino" ...
//   add --fragment to leave out <!doctype>/<head> (for hosts that add their own)
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildSketch } from './build.mjs';

const web = resolve(dirname(fileURLToPath(import.meta.url)), '..', 'web');

// Concatenate the ES modules into one inline script.
export async function inlineScripts() {
  let js = '';
  for (const f of ['engine.js', 'world.js', 'app.js']) {
    const src = await readFile(resolve(web, f), 'utf8');
    js += `// ---- ${f}\n` + src.replace(/^import .*$/gm, '').replace(/^export \{[^}]*\};?$/gm, '').replace(/^export /gm, '') + '\n';
  }
  return js.replace(/<\/script/gi, '<\\/script');
}

export function wrapDocument(fragment) {
  return `<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n</head>\n<body>\n${fragment}\n</body>\n</html>\n`;
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const args = process.argv.slice(2);
  const oi = args.indexOf('-o');
  const out = oi >= 0 ? args[oi + 1] : 'dist/simulator.html';
  const fragmentOnly = args.includes('--fragment');
  const specs = args.filter((a, i) => !a.startsWith('-') && args[i - 1] !== '-o');
  if (!specs.length) { console.error('usage: node tools/bundle.mjs [-o out.html] [--fragment] "Label=path/to/Sketch.ino" ...'); process.exit(1); }
  const firmwares = [];
  for (const spec of specs) {
    const [label, path] = spec.includes('=') ? spec.split(/=(.*)/s) : [spec, spec];
    const { wasm, warnings } = await buildSketch(path, { log: () => {} });
    firmwares.push({ id: label.toLowerCase().replace(/[^a-z0-9]+/g, '-'), label, wasm: Buffer.from(wasm).toString('base64'), notes: warnings.trim() });
    console.error(`built ${label}: ${(wasm.length / 1024).toFixed(0)} KB`);
  }
  const page = await readFile(resolve(web, 'page.html'), 'utf8');
  const fragment = page + `\n<script>window.SIM_FIRMWARES = ${JSON.stringify(firmwares)};</script>\n<script type="module">\n${await inlineScripts()}</script>\n`;
  await mkdir(dirname(resolve(out)), { recursive: true });
  await writeFile(out, fragmentOnly ? fragment : wrapDocument(fragment));
  console.error(`wrote ${out}`);
}
