// Runs a compiled sketch (WebAssembly) on a virtual clock.
// No DOM here: the browser app and the headless test runner both use it.

const G_EVENT_NAMES = new Set(['clear', 'begin', 'serial-begin', 'display-before-begin']);

export class Firmware {
  // sensor(tSeconds) -> [x, y, z] in g, as the accelerometer would feel it.
  constructor(wasmBytes, { sensor, onFrame, onSerial, onEvent } = {}) {
    this.wasmBytes = wasmBytes;
    this.sensor = sensor || (() => [0, 0, 1]);
    this.onFrame = onFrame || (() => {});
    this.onSerial = onSerial || (() => {});
    this.onEvent = onEvent || (() => {});
    this.loopOverheadUs = 50;      // time for one pass of loop() outside delays
    this.sensorReadUs = 600;       // one ADXL345 burst read over 100 kHz I2C
  }

  async start() {
    const self = this;
    const dec = new TextDecoder();
    const env = {
      sim_delay(ms) { self._pause(ms * 1000); },
      sim_now() { return self.now; },
      sim_serial_write(ptr, len) { self._serial(dec.decode(self._bytes(ptr, len))); },
      sim_random() { return self._rand(); },
      sim_analog_read() { return 0; },
      sim_digital_read() { return 0; },
      sim_digital_write() {},
      sim_accel_sample(ptr) {
        const v = self.sensor(self.now / 1e6);
        new Float64Array(self.mem.buffer, ptr, 3).set(v);
        self.lastSample = v;
        self.now += self.sensorReadUs;
      },
      sim_event(ptr, len, value) {
        const name = dec.decode(self._bytes(ptr, len));
        if (name === 'clear') { self.drawn = []; return; }
        if (name === 'serial-begin') self.baud = value;
        self.onEvent({ t: self.now / 1e6, name, value });
      },
      sim_register_bitmap(ptr, len, addr, size) {
        self.bitmapNames.set(addr, dec.decode(self._bytes(ptr, len)));
      },
      sim_draw_bitmap(addr, x, y, w, h) {
        self.drawn.push({ name: self.bitmapNames.get(addr) || `bitmap@${addr}`, x, y, w, h });
      },
      sim_display(ptr, w, h, inverted, on, contrast, transferUs) {
        if (self._resuming()) return;
        const frame = {
          t: self.now / 1e6, w, h, inverted: !!inverted, on: !!on, contrast,
          buffer: self._bytes(ptr, (w * h) >> 3).slice(),
          bitmaps: self.drawn.map(d => d.name),
        };
        self.frame = frame;
        self.frames++;
        self.onFrame(frame);
        self._pause(transferUs, true);
      },
    };
    // Anything else (WASI, unknown) gets a harmless stub.
    const wasi = new Proxy({}, { get: (_, name) => (name === 'fd_write' ? self._fdWrite.bind(self) : () => 52) });
    this.imports = { env, wasi_snapshot_preview1: wasi };
    this.module = this.module || await WebAssembly.compile(this.wasmBytes);
    await this.reboot();
    return this;
  }

  // Power-cycle: a fresh instance, starting again at setup().
  async reboot() {
    const instance = await WebAssembly.instantiate(this.module, this.imports);
    this.x = instance.exports;
    this.mem = this.x.memory;
    this.bitmapNames = new Map();
    this.reset();
    if (this.x._initialize) this.x._initialize();
    this.x.sim_register();
  }

  reset() {
    this.now = 0;            // virtual microseconds since power-on
    this.phase = 'setup';
    this.waitUntil = null;   // virtual time the current pause ends
    this.rewinding = false;
    this.frame = null;
    this.frames = 0;
    this.loops = 0;
    this.drawn = [];
    this.baud = null;
    this.serialLine = '';
    this.seed = 0x2545F491;
    this.lastSample = [0, 0, 1];
    this.error = null;
  }

  // Advance the sketch until virtual time reaches `targetUs` (or it crashes).
  runUntil(targetUs) {
    const x = this.x;
    const data = x.sim_asyncify_data(), size = x.sim_asyncify_size();
    let guard = 0;
    while (!this.error) {
      if (this.waitUntil !== null) {
        if (this.waitUntil > targetUs) { this.now = Math.max(this.now, targetUs); return; }
        this.now = Math.max(this.now, this.waitUntil);
        this.waitUntil = null;
        // Resume where delay()/display() unwound.
        x.asyncify_start_rewind(data);
        this.rewinding = true;
      } else {
        if (this.now >= targetUs) return;
        if (this.phase === 'loop') { this.loops++; }
        new Int32Array(this.mem.buffer, data, 2).set([data + 8, data + size]);
      }
      const entry = this.phase === 'setup' ? x.sim_setup : x.sim_loop;
      try {
        entry();
      } catch (e) {
        this.error = e;
        this.onEvent({ t: this.now / 1e6, name: 'crash', value: 0, message: String(e && e.message || e) });
        return;
      }
      if (x.asyncify_get_state() === 1) {
        x.asyncify_stop_unwind();
      } else {
        // setup() or loop() returned normally.
        if (this.phase === 'setup') this.phase = 'loop';
        else this.now += this.loopOverheadUs;
        if (++guard > 200000) return;  // loop() that never waits: let the page breathe
      }
    }
  }

  _pause(us, fromDisplay = false) {
    if (!fromDisplay && this._resuming()) return;
    if (us <= 0) return;
    this.waitUntil = this.now + us;
    this.x.asyncify_start_unwind(this.x.sim_asyncify_data());
  }

  _resuming() {
    if (this.rewinding) {
      this.x.asyncify_stop_rewind();
      this.rewinding = false;
      return true;
    }
    return false;
  }

  _serial(text) {
    this.serialLine += text;
    let i;
    while ((i = this.serialLine.indexOf('\n')) >= 0) {
      const line = this.serialLine.slice(0, i).replace(/\r$/, '');
      this.serialLine = this.serialLine.slice(i + 1);
      this.onSerial({ t: this.now / 1e6, line });
    }
  }

  _fdWrite(fd, iovs, iovsLen, nwritten) {
    const v = new DataView(this.mem.buffer);
    let n = 0;
    for (let i = 0; i < iovsLen; i++) {
      const p = v.getUint32(iovs + 8 * i, true), l = v.getUint32(iovs + 8 * i + 4, true);
      this._serial(new TextDecoder().decode(this._bytes(p, l)));
      n += l;
    }
    v.setUint32(nwritten, n, true);
    return 0;
  }

  _bytes(ptr, len) { return new Uint8Array(this.mem.buffer, ptr, len); }

  _rand() {
    let s = this.seed;
    s ^= s << 13; s ^= s >>> 17; s ^= s << 5;
    this.seed = s >>> 0;
    return this.seed / 4294967296;
  }

}

// Turn a 1-bit SSD1306 page buffer into a pixel getter.
export function pixelAt(frame, x, y) {
  return (frame.buffer[x + (y >> 3) * frame.w] >> (y & 7)) & 1;
}

// Short label for what's on screen, e.g. "slideRight2" or "shocked + text".
export function frameLabel(frame) {
  if (!frame) return 'off';
  if (!frame.on) return 'display off';
  const names = [...new Set(frame.bitmaps)];
  if (names.length) return names.join(' + ');
  const lit = frame.buffer.some(b => b);
  return lit ? 'drawing' : 'blank';
}

export { G_EVENT_NAMES };
