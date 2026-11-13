#!/usr/bin/env node
// Live simulator for a sketch you are editing.
//
//   npm run sim -- ../firmware/CarCompanion/CarCompanion.ino
//
// Builds the sketch, serves the simulator at http://localhost:8137 and rebuilds
// and reloads the firmware every time you save the .ino.
import { createServer } from 'node:http';
import { readFile, stat } from 'node:fs/promises';
import { watch } from 'node:fs';
import { basename, dirname, extname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { exec } from 'node:child_process';
import { buildSketch } from './build.mjs';
import { wrapDocument } from './bundle.mjs';

const web = resolve(dirname(fileURLToPath(import.meta.url)), '..', 'web');
const args = process.argv.slice(2);
const ino = args.find(a => !a.startsWith('-'));
const port = +(args[args.indexOf('--port') + 1] || 8137) || 8137;
if (!ino) { console.error('usage: npm run sim -- path/to/Sketch.ino [--port 8137]'); process.exit(1); }

let firmware = null, notes = '', lastError = null;
const clients = new Set();

async function rebuild() {
  const t = Date.now();
  try {
    const r = await buildSketch(ino, { log: () => {} });
    firmware = r.wasm; notes = r.warnings.trim(); lastError = null;
    console.log(`built ${basename(ino)} in ${((Date.now() - t) / 1000).toFixed(1)} s` + (notes ? ' (with warnings)' : ''));
    send({ ok: true, notes });
  } catch (e) {
    lastError = e.compilerOutput || e.message;
    console.log(lastError);
    send({ ok: false, error: lastError });
  }
}
function send(data) { for (const res of clients) res.write(`event: build\ndata: ${JSON.stringify(data)}\n\n`); }

const TYPES = { '.js': 'text/javascript', '.html': 'text/html', '.css': 'text/css', '.wasm': 'application/wasm' };
const server = createServer(async (req, res) => {
  const url = new URL(req.url, 'http://x');
  if (url.pathname === '/') {
    const page = await readFile(resolve(web, 'page.html'), 'utf8');
    const local = { name: basename(ino), notes };
    res.writeHead(200, { 'content-type': 'text/html' });
    res.end(wrapDocument(page + `\n<script>window.SIM_LOCAL = ${JSON.stringify(local)};</script>\n<script type="module" src="app.js"></script>\n` +
      (lastError ? `<script>addEventListener('load', () => setTimeout(() => { const m = document.getElementById('msg'); m.hidden = false; m.textContent = ${JSON.stringify('Build failed:\n' + lastError)}; }, 300));</script>` : '')));
    return;
  }
  if (url.pathname === '/firmware.wasm') {
    if (!firmware) { res.writeHead(404); res.end(); return; }
    res.writeHead(200, { 'content-type': 'application/wasm', 'cache-control': 'no-store' });
    res.end(firmware); return;
  }
  if (url.pathname === '/events') {
    res.writeHead(200, { 'content-type': 'text/event-stream', 'cache-control': 'no-store', connection: 'keep-alive' });
    res.write(': hi\n\n');
    clients.add(res);
    req.on('close', () => clients.delete(res));
    return;
  }
  const file = resolve(web, '.' + url.pathname);
  if (!file.startsWith(web)) { res.writeHead(403); res.end(); return; }
  try {
    await stat(file);
    res.writeHead(200, { 'content-type': TYPES[extname(file)] || 'application/octet-stream', 'cache-control': 'no-store' });
    res.end(await readFile(file));
  } catch { res.writeHead(404); res.end('not found'); }
});

await rebuild();
server.listen(port, '127.0.0.1', () => {
  const url = `http://localhost:${port}`;
  console.log(`Car Companion simulator: ${url}  (watching ${ino}, Ctrl+C to stop)`);
  if (!args.includes('--no-open')) {
    const cmd = process.platform === 'darwin' ? 'open' : process.platform === 'win32' ? 'start ""' : 'xdg-open';
    exec(`${cmd} ${url}`, () => {});
  }
});

// Rebuild when the sketch (or a file next to it) changes. Debounced, because
// editors often write a file in several steps.
let timer = null;
watch(dirname(resolve(ino)), (_, f) => {
  if (f && !/\.(ino|h|hpp|c|cpp)$/.test(f)) return;
  clearTimeout(timer);
  timer = setTimeout(rebuild, 250);
});
