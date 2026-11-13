import { Firmware, pixelAt, frameLabel } from './engine.js';
import { SCENARIOS, DEFAULT_SETTINGS, CARS, scenarioAt, scenarioDuration, Car, Sensor } from './world.js';

const $ = (id) => document.getElementById(id);
const store = {
  get(k, d) { try { const v = localStorage.getItem('ccsim.' + k); return v == null ? d : JSON.parse(v); } catch { return d; } },
  set(k, v) { try { localStorage.setItem('ccsim.' + k, JSON.stringify(v)); } catch {} },
};

// ---------------------------------------------------------------- state
const SPEEDS = [0.25, 0.5, 1, 2, 4, 8];
const S = {
  fw: null,             // Firmware
  firmwares: [],        // [{ id, label, bytes, notes }]
  fwId: null,
  playing: true,
  speed: 1,
  mode: 'drives',       // drives | drive | pad
  scenario: SCENARIOS[0],
  scenarioSpeed: 0, scenarioLastT: 0,
  settings: { ...DEFAULT_SETTINGS },
  sensor: null,
  car: new Car(),
  keys: { up: false, down: false, left: false, right: false },
  pad: { lon: 0, lat: 0, dragging: false },
  motion: { lon: 0, lat: 0, grade: 0, speed: 0, potholes: 0, label: '' },
  history: [],          // { t, x, y }: what the sensor feels
  samples: [],          // { t, x, y }: what the sketch actually read
  faces: [],            // { t0, t1, face }
  serial: [],           // { t, line, cls }
  lastFrame: null,
  lastBranch: '',
  warnings: new Set(),
};

// ---------------------------------------------------------------- firmware list
async function loadFirmwares() {
  const list = [];
  for (const f of (window.SIM_FIRMWARES || [])) {
    list.push({ id: f.id, label: f.label, bytes: base64ToBytes(f.wasm), notes: f.notes || '' });
  }
  if (window.SIM_LOCAL) {
    try {
      const r = await fetch('firmware.wasm?' + Date.now());
      if (r.ok) list.unshift({ id: 'local', label: 'Local: ' + (window.SIM_LOCAL.name || 'sketch'), bytes: new Uint8Array(await r.arrayBuffer()), notes: window.SIM_LOCAL.notes || '' });
    } catch {}
  }
  S.firmwares = list;
  renderFirmwareSelect();
}

function base64ToBytes(b64) {
  const bin = atob(b64), out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
  return out;
}

function renderFirmwareSelect() {
  const sel = $('fw');
  sel.innerHTML = '';
  for (const f of S.firmwares) sel.append(new Option(f.label, f.id));
  sel.append(new Option('Load a .wasm build…', '__file'));
  const want = store.get('fw', null);
  S.fwId = S.firmwares.find(f => f.id === want)?.id ?? S.firmwares[0]?.id;
  if (window.SIM_LOCAL) S.fwId = 'local';
  sel.value = S.fwId ?? '__file';
}

$('fw').addEventListener('change', async (e) => {
  if (e.target.value === '__file') { $('fwfile').click(); e.target.value = S.fwId; return; }
  S.fwId = e.target.value;
  store.set('fw', S.fwId);
  await bootFirmware();
});
$('fwfile').addEventListener('change', async (e) => {
  const file = e.target.files[0];
  if (!file) return;
  const id = 'file:' + file.name;
  S.firmwares = S.firmwares.filter(f => f.id !== id);
  S.firmwares.push({ id, label: 'File: ' + file.name, bytes: new Uint8Array(await file.arrayBuffer()), notes: '' });
  renderFirmwareSelect();
  S.fwId = id; $('fw').value = id;
  await bootFirmware();
});

// ---------------------------------------------------------------- boot / power
async function bootFirmware() {
  const entry = S.firmwares.find(f => f.id === S.fwId);
  if (!entry) { showMsg('No firmware loaded.\nBuild one with: npm run build -- Sketch.ino'); return; }
  try {
    S.fw = new Firmware(entry.bytes, {
      sensor: (t) => {
        const v = S.sensor.read(t, motionAt(t));
        S.samples.push({ t, x: v[0], y: v[1] });
        if (S.samples.length > 4000) S.samples.splice(0, 1000);
        return v;
      },
      onFrame: onFrame,
      onSerial: onSerial,
      onEvent: onEvent,
    });
    await S.fw.start();
  } catch (e) {
    S.fw = null;
    showMsg('This firmware could not start:\n' + (e.message || e));
    return;
  }
  renderNotes(entry);
  powerOn();
}

function powerOn() {
  if (!S.fw) return;
  S.fw.reboot().then(() => {
    const st = { ...S.settings, ...(S.mode === 'drives' ? S.scenario.settings : {}) };
    S.sensor = new Sensor(st, 7);
    S.traceSensor = new Sensor(st, 99);
    S.samples = [];
    S.car.spec = CARS[S.carType] || CARS.family;
    S.car.reset();
    S.scenarioSpeed = 0; S.scenarioLastT = 0;
    S.history = []; S.faces = []; S.serial = []; S.lastFrame = null; S.lastBranch = '';
    S.warnings.clear();
    $('serial').textContent = '';
    showMsg('');
    S.wallBase = performance.now(); S.simBase = 0;
    drawOled(null);
    renderFlags();
  });
}

$('power').addEventListener('click', powerOn);
$('play').addEventListener('click', () => {
  S.playing = !S.playing;
  $('play').textContent = S.playing ? 'Pause' : 'Play';
  S.wallBase = performance.now(); S.simBase = S.fw ? S.fw.now : 0;
});

// ---------------------------------------------------------------- speed
for (const sp of SPEEDS) {
  const b = document.createElement('button');
  b.type = 'button'; b.textContent = sp < 1 ? `${sp}×` : `${sp}×`; b.dataset.speed = sp;
  b.addEventListener('click', () => setSpeed(sp));
  $('speed').append(b);
}
function setSpeed(sp) {
  S.speed = sp;
  for (const b of $('speed').children) b.setAttribute('aria-pressed', String(+b.dataset.speed === sp));
  S.wallBase = performance.now(); S.simBase = S.fw ? S.fw.now : 0;
  store.set('speed', sp);
}

// ---------------------------------------------------------------- motion sources
function motionAt(t) {
  if (S.mode === 'drives') {
    const m = scenarioAt(S.scenario, t);
    if (t > S.scenarioLastT) { S.scenarioSpeed = Math.max(0, S.scenarioSpeed + m.lon * 9.81 * (t - S.scenarioLastT)); S.scenarioLastT = t; }
    S.motion = { ...m, speed: S.scenarioSpeed };
  } else if (S.mode === 'drive') {
    S.motion = { lon: S.car.lon, lat: S.car.lat, grade: 0, speed: S.car.v, potholes: 0, label: '' };
  } else {
    S.motion = { lon: S.pad.lon, lat: S.pad.lat, grade: 0, speed: Math.abs(S.pad.lon) + Math.abs(S.pad.lat) > 0.02 ? 12 : 0, potholes: 0, label: '' };
  }
  return S.motion;
}

// ---------------------------------------------------------------- firmware callbacks
function onFrame(f) {
  S.lastFrame = f;
  const face = frameLabel(f);
  const last = S.faces[S.faces.length - 1];
  if (!last || last.face !== face) {
    if (last) last.t1 = f.t;
    S.faces.push({ t0: f.t, t1: null, face });
    if (S.faces.length > 600) S.faces.splice(0, 100);
  }
}

const QUIET = /X Acceleration:|Sensitivity:|forward g:/;
function onSerial({ t, line }) {
  if (!QUIET.test(line)) S.lastBranch = line.trim();
  S.serial.push({ t, line });
  if (S.serial.length > 800) S.serial.splice(0, 200);
  S.serialDirty = true;
}

function onEvent(e) {
  if (e.name === 'crash') {
    S.serial.push({ t: e.t, line: 'Sketch stopped: ' + e.message, cls: 'err' });
    S.serialDirty = true;
    S.warnings.add('crash');
    renderFlags();
  }
  if (e.name === 'display-before-begin') { S.warnings.add('nobegin'); renderFlags(); }
  if (e.name === 'serial-begin') renderFlags();
}

// ---------------------------------------------------------------- main loop
let lastWall = performance.now();
function tick(now) {
  const dtWall = Math.min(0.1, (now - lastWall) / 1000);
  lastWall = now;
  if (S.fw && S.playing && !S.fw.error) {
    const target = S.simBase + (now - S.wallBase) * 1000 * S.speed;
    // Step the keyboard car in virtual time, a few ms at a time.
    let t = S.fw.now;
    const end = Math.max(t, Math.min(target, t + 200000 * S.speed));
    if (end - t > 200000 * S.speed) { S.wallBase = now; S.simBase = end; }
    const slice = 5000;
    while (t < end) {
      const nt = Math.min(end, t + slice);
      if (S.mode === 'drive') S.car.step((nt - t) / 1e6, S.keys);
      S.fw.runUntil(nt);
      t = nt;
      sampleHistory(t / 1e6);
    }
  }
  if (S.mode === 'pad' && !S.pad.dragging && $('spring').checked && (S.pad.lon || S.pad.lat)) {
    const k = Math.exp(-dtWall * 6);
    S.pad.lon = Math.abs(S.pad.lon * k) < 0.005 ? 0 : S.pad.lon * k;
    S.pad.lat = Math.abs(S.pad.lat * k) < 0.005 ? 0 : S.pad.lat * k;
    syncPadSliders();
  }
  render();
  requestAnimationFrame(tick);
}

let lastHist = -1;
function sampleHistory(t) {
  if (t - lastHist < 0.02) return;
  lastHist = t;
  const v = S.traceSensor ? S.traceSensor.read(t, motionAt(t)) : [0, 0, 1];
  S.history.push({ t, x: v[0], y: v[1], z: v[2], lon: S.motion.lon, lat: S.motion.lat });
  while (S.history.length && S.history[0].t < t - 32) S.history.shift();
}

// ---------------------------------------------------------------- rendering
const oledCtx = $('oled').getContext('2d');
const oledImg = oledCtx.createImageData(128, 64);
function drawOled(f) {
  const d = oledImg.data;
  const on = [216, 236, 255], off = [5, 7, 10];
  for (let y = 0; y < 64; y++) for (let x = 0; x < 128; x++) {
    let lit = f && f.on ? pixelAt(f, x, y) : 0;
    if (f && f.inverted) lit ^= 1;
    if (f && !f.on) lit = 0;
    const c = lit ? on : off, i = (y * 128 + x) * 4;
    const dim = f && f.contrast < 0x40 ? 0.55 : 1;
    d[i] = c[0] * (lit ? dim : 1); d[i + 1] = c[1] * (lit ? dim : 1); d[i + 2] = c[2] * (lit ? dim : 1); d[i + 3] = 255;
  }
  oledCtx.putImageData(oledImg, 0, 0);
}

// The companion's body bounces with the road (vertical g), so you can see
// whether the face reacts in sync.
const reduceMotion = matchMedia('(prefers-reduced-motion: reduce)').matches;
let shakeX = 0;
function shakeBezel() {
  const h = S.history[S.history.length - 1];
  const el = $('bezel');
  if (!h || reduceMotion || !S.playing) { el.style.transform = ''; return; }
  const dz = Math.max(-1, Math.min(1, h.z - 1));
  shakeX = Math.abs(dz) > 0.05 ? (Math.random() * 2 - 1) * Math.abs(dz) * 5 : shakeX * 0.6;
  el.style.transform = `translate(${shakeX.toFixed(1)}px, ${(-dz * 10).toFixed(1)}px)`;
}

function showMsg(text) { $('msg').textContent = text; $('msg').hidden = !text; }

let drawnFrame = null;
function render() {
  if (S.lastFrame !== drawnFrame) { drawOled(S.lastFrame); drawnFrame = S.lastFrame; }
  shakeBezel();
  const t = S.fw ? S.fw.now / 1e6 : 0;
  $('clock').textContent = t.toFixed(1) + ' s';
  $('face').textContent = S.lastFrame ? frameLabel(S.lastFrame) : '–';
  $('branch').textContent = S.lastBranch ? 'Serial: ' + S.lastBranch : '';
  const m = S.motion;
  $('r-speed').textContent = Math.round((m.speed || 0) * 3.6);
  $('r-lon').textContent = fmt(m.lon);
  $('r-lat').textContent = fmt(m.lat);
  const v = S.history[S.history.length - 1];
  $('r-xy').textContent = v ? `${fmt(v.x)} ${fmt(v.y)}` : '–';
  if (S.mode === 'drives') renderProgress(t);
  if (S.mode === 'pad') drawPad();
  if (S.mode === 'drive') drawTrack();
  drawTrace(t);
  if (S.serialDirty) renderSerial();
}
const fmt = (n) => (n >= 0 ? '+' : '') + (n || 0).toFixed(2);

function renderFlags() {
  const el = $('flags');
  el.innerHTML = '';
  const add = (cls, text) => { const s = document.createElement('span'); s.className = 'chip ' + cls; s.textContent = text; el.append(s); };
  if (S.fw && S.fw.baud) add(S.fw.baud === 115200 || S.fw.baud === 9600 ? 'ok' : 'warn', `Serial.begin(${S.fw.baud})` + (S.fw.baud === 115200 || S.fw.baud === 9600 ? '' : ': unusual baud rate'));
  if (S.warnings.has('crash')) add('bad', 'Sketch crashed, see Serial');
  if (S.warnings.has('nobegin')) add('bad', 'display() before display.begin()');
  const st = { ...S.settings, ...(S.mode === 'drives' ? S.scenario.settings : {}) };
  if (st.mountPitch || st.mountRoll || st.mountYaw) add('warn', `Mount tilted ${st.mountPitch}° / ${st.mountRoll}° / twisted ${st.mountYaw}°`);
  if (st.flipX || st.flipY) add('warn', 'Sensor axis flipped');
}

// scripted drive progress
function renderDrives() {
  const el = $('drives');
  el.innerHTML = '';
  for (const sc of SCENARIOS) {
    const b = document.createElement('button');
    b.type = 'button'; b.className = 'drive';
    b.setAttribute('aria-pressed', String(sc === S.scenario));
    b.innerHTML = `<strong></strong><span></span>`;
    b.querySelector('strong').textContent = `${sc.name} · ${Math.round(scenarioDuration(sc))} s`;
    b.querySelector('span').textContent = sc.about;
    b.addEventListener('click', () => { S.scenario = sc; store.set('drive', sc.id); renderDrives(); renderSteps(); powerOn(); });
    el.append(b);
  }
}
function renderSteps() {
  const el = $('steps');
  el.innerHTML = '';
  const total = scenarioDuration(S.scenario);
  for (const s of S.scenario.steps) {
    const d = document.createElement('div');
    d.style.width = (s.dur / total * 100) + '%';
    d.textContent = s.label; d.title = s.label;
    el.append(d);
  }
  const c = document.createElement('div'); c.className = 'cursor'; c.id = 'cursor'; el.append(c);
}
function renderProgress(t) {
  const total = scenarioDuration(S.scenario);
  const c = $('cursor');
  if (c) c.style.left = `calc(${Math.min(1, t / total) * 100}% - 1px)`;
  const m = scenarioAt(S.scenario, t);
  $('stepnow').textContent = t >= total ? 'Drive finished. The car is parked; pick another drive or power cycle to replay.' : `Now: ${m.label}`;
}

// g pad
const padCtx = $('pad').getContext('2d');
function drawPad() {
  const c = padCtx, W = c.canvas.width, R = W / 2 - 16, cx = W / 2, cy = W / 2;
  c.clearRect(0, 0, W, W);
  c.fillStyle = '#22262c'; c.beginPath(); c.arc(cx, cy, R, 0, Math.PI * 2); c.fill();
  c.strokeStyle = '#3a4048'; c.lineWidth = 2;
  for (const g of [0.5, 0.7, 1]) { c.beginPath(); c.arc(cx, cy, R * g, 0, Math.PI * 2); c.stroke(); }
  c.setLineDash([8, 8]); c.strokeStyle = '#6b737c';
  const tr = R * 0.3;
  c.strokeRect(cx - tr, cy - tr, tr * 2, tr * 2);
  c.setLineDash([]);
  c.strokeStyle = '#3a4048'; c.beginPath(); c.moveTo(cx - R, cy); c.lineTo(cx + R, cy); c.moveTo(cx, cy - R); c.lineTo(cx, cy + R); c.stroke();
  c.fillStyle = '#8b939c'; c.font = '600 22px "Barlow Semi Condensed", sans-serif'; c.textAlign = 'center';
  c.fillText('GAS', cx, cy - R + 26); c.fillText('BRAKE', cx, cy + R - 10);
  c.textAlign = 'left'; c.fillText('LEFT', cx - R + 8, cy - 8); c.textAlign = 'right'; c.fillText('RIGHT', cx + R - 8, cy - 8);
  c.textAlign = 'left'; c.font = '18px "JetBrains Mono", monospace';
  c.fillText('1 g', cx + R * 0.71 + 2, cy - R * 0.71);
  // trail from history
  c.strokeStyle = 'rgba(255,181,71,.35)'; c.lineWidth = 2; c.beginPath();
  const h = S.history.slice(-60);
  h.forEach((p, i) => { const x = cx + p.lat * R, y = cy - p.lon * R; i ? c.lineTo(x, y) : c.moveTo(x, y); });
  c.stroke();
  const x = cx + S.pad.lat * R, y = cy - S.pad.lon * R;
  c.fillStyle = '#ffb547'; c.beginPath(); c.arc(x, y, 14, 0, Math.PI * 2); c.fill();
}
function padFromEvent(e) {
  const r = $('pad').getBoundingClientRect();
  const R = (r.width / 2) * ((520 / 2 - 16) / (520 / 2));
  let lat = (e.clientX - r.left - r.width / 2) / R, lon = -(e.clientY - r.top - r.height / 2) / R;
  const m = Math.hypot(lat, lon);
  if (m > 1) { lat /= m; lon /= m; }
  S.pad.lat = round2(lat); S.pad.lon = round2(lon);
  syncPadSliders();
}
const round2 = (n) => Math.round(n * 100) / 100;
$('pad').addEventListener('pointerdown', (e) => { S.pad.dragging = true; $('pad').setPointerCapture(e.pointerId); padFromEvent(e); });
$('pad').addEventListener('pointermove', (e) => { if (S.pad.dragging) padFromEvent(e); });
$('pad').addEventListener('pointerup', () => { S.pad.dragging = false; });
$('pad').addEventListener('pointercancel', () => { S.pad.dragging = false; });
for (const k of ['lon', 'lat']) {
  $('s-' + k).addEventListener('input', (e) => { S.pad[k] = +e.target.value; S.pad.dragging = true; syncPadSliders(); });
  $('s-' + k).addEventListener('change', () => { S.pad.dragging = false; });
}
function syncPadSliders() {
  for (const k of ['lon', 'lat']) { $('s-' + k).value = S.pad[k]; $('v-' + k).textContent = fmt(S.pad[k]); }
}

// keyboard car
const KEYMAP = { ArrowUp: 'up', KeyW: 'up', ArrowDown: 'down', KeyS: 'down', ArrowLeft: 'left', KeyA: 'left', ArrowRight: 'right', KeyD: 'right' };
addEventListener('keydown', (e) => {
  if (S.mode !== 'drive' || e.target.closest('input, select')) return;
  const k = KEYMAP[e.code];
  if (k) { S.keys[k] = true; e.preventDefault(); syncPedals(); }
});
addEventListener('keyup', (e) => { const k = KEYMAP[e.code]; if (k) { S.keys[k] = false; syncPedals(); } });
addEventListener('blur', () => { for (const k in S.keys) S.keys[k] = false; syncPedals(); });
for (const b of document.querySelectorAll('.pedals button')) {
  const k = b.dataset.key;
  b.addEventListener('pointerdown', (e) => { S.keys[k] = true; b.setPointerCapture(e.pointerId); syncPedals(); });
  const up = () => { S.keys[k] = false; syncPedals(); };
  b.addEventListener('pointerup', up); b.addEventListener('pointercancel', up);
  b.addEventListener('contextmenu', (e) => e.preventDefault());
}
function renderCars() {
  const el = $('cars');
  el.innerHTML = '';
  for (const [id, spec] of Object.entries(CARS)) {
    const b = document.createElement('button');
    b.type = 'button';
    b.textContent = `${spec.name} · ${spec.maxG} g`;
    b.setAttribute('aria-pressed', String(id === S.carType));
    b.addEventListener('click', () => { S.carType = id; S.car.spec = spec; store.set('car', id); renderCars(); });
    el.append(b);
  }
}

function syncPedals() { for (const b of document.querySelectorAll('.pedals button')) b.classList.toggle('on', S.keys[b.dataset.key]); }

const trail = [];
function drawTrack() {
  const cv = $('track'), c = cv.getContext('2d');
  const W = cv.clientWidth, H = cv.clientHeight, dpr = devicePixelRatio || 1;
  if (cv.width !== W * dpr) { cv.width = W * dpr; cv.height = H * dpr; }
  c.setTransform(dpr, 0, 0, dpr, 0, 0);
  const car = S.car;
  const last = trail[trail.length - 1];
  if (!last || Math.hypot(last.x - car.x, last.y - car.y) > 0.5) { trail.push({ x: car.x, y: car.y }); if (trail.length > 600) trail.shift(); }
  c.fillStyle = '#22262c'; c.fillRect(0, 0, W, H);
  const scale = 1.2;
  c.save(); c.translate(W / 2, H / 2 + 30);
  c.rotate(car.heading);
  c.strokeStyle = 'rgba(255,181,71,.5)'; c.lineWidth = 3; c.beginPath();
  trail.forEach((p, i) => { const x = (p.x - car.x) * scale, y = -(p.y - car.y) * scale; i ? c.lineTo(x, y) : c.moveTo(x, y); });
  c.stroke(); c.restore();
  c.save(); c.translate(W / 2, H / 2 + 30);
  c.fillStyle = '#e6e8ea'; c.beginPath(); c.roundRect ? c.roundRect(-6, -11, 12, 22, 3) : c.rect(-6, -11, 12, 22); c.fill();
  c.restore();
  c.fillStyle = '#8b939c'; c.font = '12px "JetBrains Mono", monospace';
  c.fillText(`gas ${Math.round(car.throttle * 100)}%  brake ${Math.round(car.brake * 100)}%  steer ${car.steer >= 0 ? '+' : ''}${car.steer.toFixed(2)}`, 10, 18);
}

// trace
function faceColor(name) {
  if (/left/i.test(name)) return 'hsl(200 55% 36%)';
  if (/right/i.test(name)) return 'hsl(280 40% 40%)';
  if (/smile|default|content|neutral|joy/i.test(name)) return 'hsl(150 28% 30%)';
  if (/sleep|asleep/i.test(name)) return 'hsl(235 30% 34%)';
  if (/shock|speed/i.test(name)) return 'hsl(35 70% 36%)';
  if (/stop|brak/i.test(name)) return 'hsl(4 60% 40%)';
  let h = 0;
  for (const ch of name) h = (h * 31 + ch.charCodeAt(0)) >>> 0;
  return `hsl(${h % 360} 35% 34%)`;
}
function drawTrace(t) {
  const cv = $('trace'), c = cv.getContext('2d');
  const W = cv.clientWidth, H = cv.clientHeight, dpr = devicePixelRatio || 1;
  if (cv.width !== Math.round(W * dpr)) { cv.width = Math.round(W * dpr); cv.height = Math.round(H * dpr); }
  c.setTransform(dpr, 0, 0, dpr, 0, 0);
  c.clearRect(0, 0, W, H);
  const left = 34, right = W - 6, top = 6, stripH = 22, bottom = H - stripH - 20;
  const t0 = Math.max(0, t - 30), t1 = Math.max(30, t);
  const X = (tt) => left + (tt - t0) / (t1 - t0) * (right - left);
  const G = 1.2, Y = (g) => top + (G - g) / (2 * G) * (bottom - top);
  c.font = '11px "JetBrains Mono", monospace'; c.fillStyle = '#8b939c'; c.textAlign = 'right';
  c.strokeStyle = '#2e333a'; c.lineWidth = 1;
  for (const g of [-1, -0.5, 0, 0.5, 1]) { c.beginPath(); c.moveTo(left, Y(g)); c.lineTo(right, Y(g)); c.stroke(); c.fillText(g.toFixed(1), left - 4, Y(g) + 4); }
  c.setLineDash([4, 4]); c.strokeStyle = '#6b737c';
  for (const g of [-0.3, 0.3]) { c.beginPath(); c.moveTo(left, Y(g)); c.lineTo(right, Y(g)); c.stroke(); }
  c.setLineDash([]);
  const css = getComputedStyle(document.documentElement);
  // Vertical: draw Z minus gravity faintly, so bumps are visible.
  c.strokeStyle = css.getPropertyValue('--z').trim(); c.lineWidth = 1; c.beginPath();
  { let first = true;
    for (const p of S.history) { if (p.t < t0 || p.z === undefined) continue; const x = X(p.t), y = Y(Math.max(-G, Math.min(G, p.z - 1))); first ? c.moveTo(x, y) : c.lineTo(x, y); first = false; } }
  c.stroke();
  for (const [key, color] of [['x', css.getPropertyValue('--x')], ['y', css.getPropertyValue('--y')]]) {
    c.strokeStyle = color.trim(); c.lineWidth = 1.5; c.beginPath();
    let first = true;
    for (const p of S.history) { if (p.t < t0) continue; const x = X(p.t), y = Y(Math.max(-G, Math.min(G, p[key]))); first ? c.moveTo(x, y) : c.lineTo(x, y); first = false; }
    c.stroke();
    c.fillStyle = color.trim();
    for (const p of S.samples) { if (p.t < t0) continue; c.beginPath(); c.arc(X(p.t), Y(Math.max(-G, Math.min(G, p[key]))), 2.2, 0, Math.PI * 2); c.fill(); }
  }
  // face strip
  const sy = H - stripH - 14;
  c.textAlign = 'left';
  for (const f of S.faces) {
    const a = Math.max(t0, f.t0), b = Math.min(t, f.t1 ?? t);
    if (b <= a) continue;
    const x0 = X(a), x1 = X(b);
    c.fillStyle = faceColor(f.face); c.fillRect(x0, sy, Math.max(1, x1 - x0 - 1), stripH);
    if (x1 - x0 > 40) { c.save(); c.beginPath(); c.rect(x0, sy, x1 - x0 - 2, stripH); c.clip(); c.fillStyle = '#e6e8ea'; c.fillText(f.face, x0 + 4, sy + 15); c.restore(); }
  }
  c.fillStyle = '#8b939c'; c.textAlign = 'center';
  for (let s = Math.ceil(t0 / 5) * 5; s <= t1; s += 5) c.fillText(s + 's', X(s), H - 2);
  c.textAlign = 'left'; c.fillText('screen', 0, sy + 15);
}

// serial
function renderSerial() {
  S.serialDirty = false;
  const el = $('serial');
  const atBottom = el.scrollTop + el.clientHeight >= el.scrollHeight - 30;
  const quiet = $('quiet').checked;
  const rows = S.serial.filter(r => !(quiet && QUIET.test(r.line))).slice(-300);
  const frag = document.createDocumentFragment();
  for (const r of rows) {
    const line = document.createElement('div');
    const ts = document.createElement('span'); ts.className = 't'; ts.textContent = r.t.toFixed(2).padStart(7) + '  ';
    const tx = document.createElement('span'); tx.textContent = r.line;
    if (r.cls) tx.className = r.cls; else if (!QUIET.test(r.line)) tx.className = 'hl';
    line.append(ts, tx); frag.append(line);
  }
  el.replaceChildren(frag);
  if (atBottom) el.scrollTop = el.scrollHeight;
}
$('quiet').addEventListener('change', renderSerial);

// tabs
const TABS = { drives: 'tab-drives', drive: 'tab-drive', pad: 'tab-pad' };
function setMode(mode, reboot = true) {
  S.mode = mode;
  store.set('mode', mode);
  for (const [m, id] of Object.entries(TABS)) {
    $(id).setAttribute('aria-selected', String(m === mode));
    $('pane-' + m).hidden = m !== mode;
  }
  if (reboot) powerOn();
  renderFlags();
}
for (const [m, id] of Object.entries(TABS)) $(id).addEventListener('click', () => setMode(m, m === 'drives' || S.mode === 'drives'));

// settings
const SETTINGS_UI = [
  ['mountPitch', 'Leaning back', -30, 30, 1, '°'],
  ['mountRoll', 'Leaning sideways', -30, 30, 1, '°'],
  ['mountYaw', 'Twisted on the dash', -45, 45, 1, '°'],
  ['noise', 'Sensor noise', 0, 0.1, 0.005, ' g'],
  ['road', 'Road roughness', 0, 1, 0.05, ''],
];
function renderSettings() {
  const el = $('settings-grid');
  el.innerHTML = '';
  for (const [key, label, min, max, step, unit] of SETTINGS_UI) {
    const w = document.createElement('div'); w.className = 'slider';
    w.innerHTML = `<div class="row"><label for="set-${key}"></label><span class="mono" id="val-${key}"></span></div><input type="range" id="set-${key}" min="${min}" max="${max}" step="${step}">`;
    w.querySelector('label').textContent = label;
    const input = w.querySelector('input');
    input.value = S.settings[key];
    const show = () => { w.querySelector('.mono').textContent = (+S.settings[key]).toFixed(step < 1 ? (step < 0.01 ? 3 : 2) : 0) + unit; };
    show();
    input.addEventListener('input', () => { S.settings[key] = +input.value; show(); applySettings(); });
    el.append(w);
  }
  for (const [key, label] of [['flipX', 'Flip sensor X'], ['flipY', 'Flip sensor Y'], ['bodyMotion', 'Body dive and roll']]) {
    const l = document.createElement('label'); l.className = 'check';
    l.innerHTML = `<input type="checkbox" id="set-${key}"> <span></span>`;
    l.querySelector('span').textContent = label;
    const input = l.querySelector('input');
    input.checked = S.settings[key];
    input.addEventListener('change', () => { S.settings[key] = input.checked; applySettings(); });
    el.append(l);
  }
  const reset = document.createElement('button');
  reset.type = 'button'; reset.className = 'btn'; reset.textContent = 'Reset to defaults';
  reset.addEventListener('click', () => { S.settings = { ...DEFAULT_SETTINGS }; renderSettings(); applySettings(); });
  el.append(reset);
}
function applySettings() {
  store.set('settings', S.settings);
  for (const sn of [S.sensor, S.traceSensor]) if (sn) Object.assign(sn.settings, S.settings, S.mode === 'drives' ? S.scenario.settings : {});
  renderFlags();
}

function renderNotes(entry) {
  const el = $('buildnotes');
  if (!entry.notes) { el.hidden = true; return; }
  el.hidden = false;
  el.innerHTML = '<span class="label">Compiler notes for this build</span><pre></pre>';
  el.querySelector('pre').textContent = entry.notes;
}

// ---------------------------------------------------------------- start
(async function main() {
  S.settings = { ...DEFAULT_SETTINGS, ...store.get('settings', {}) };
  S.scenario = SCENARIOS.find(s => s.id === store.get('drive', 'lane')) || SCENARIOS[3];
  setSpeed(store.get('speed', 1));
  S.carType = CARS[store.get('car', 'family')] ? store.get('car', 'family') : 'family';
  S.car.spec = CARS[S.carType];
  renderCars();
  renderDrives(); renderSteps(); renderSettings(); syncPadSliders();
  setMode(store.get('mode', 'drives'), false);
  await loadFirmwares();
  await bootFirmware();
  requestAnimationFrame(tick);
  if (window.SIM_LOCAL) {
    const es = new EventSource('events');
    es.addEventListener('build', async (e) => {
      const info = JSON.parse(e.data);
      if (!info.ok) { showMsg('Build failed:\n' + info.error); return; }
      window.SIM_LOCAL.notes = info.notes;
      await loadFirmwares();
      S.fwId = 'local'; $('fw').value = 'local';
      await bootFirmware();
    });
  }
})();
