// The simulated car and the accelerometer on its dashboard.
//
// Car frame: forward, right, up. "lon" is forward acceleration in g
// (+ speeding up, - braking); "lat" is sideways acceleration in g
// (+ turning right, - turning left).
//
// Default mounting matches what the firmware expects: sensor X points to the
// back of the car (so accelerating reads X < 0 and braking X > 0), Y points
// right (right turns read Y > 0), Z points up (+1 g at rest).

const DEG = Math.PI / 180;

export const DEFAULT_SETTINGS = {
  mountPitch: 0,     // deg, screen leaning back toward the driver
  mountRoll: 0,      // deg, leaning toward the passenger side
  mountYaw: 0,       // deg, twisted on the dash
  flipX: false,
  flipY: false,
  noise: 0.01,       // g, sensor noise (ADXL345 is about 0.01 g)
  road: 0.3,         // 0..1 road roughness (vibration and bumps)
  bodyMotion: true,  // body dives under braking, squats and rolls
};

// Built-in drives. Times in seconds; each step ramps to its lon/lat over `ramp`
// and holds for `dur`. Every drive begins parked so the startup animation
// (and the V0.4 level calibration) happens with the car still.
export const SCENARIOS = [
  {
    id: 'city', name: 'Calm city drive',
    about: 'Gentle pulling away, a soft right turn and an easy stop. A good companion stays calm.',
    steps: [
      { dur: 9, label: 'Parked' },
      { dur: 4, lon: 0.15, label: 'Pull away' },
      { dur: 3, label: 'Cruise' },
      { dur: 3, lat: 0.2, label: 'Soft right turn' },
      { dur: 3, label: 'Cruise' },
      { dur: 3, lat: -0.2, label: 'Soft left turn' },
      { dur: 3, lon: -0.18, label: 'Easy stop' },
      { dur: 4, label: 'Stopped' },
    ],
  },
  {
    id: 'launch', name: 'Hard launch',
    about: 'Floors it from a standstill, then eases off.',
    steps: [
      { dur: 9, label: 'Parked' },
      { dur: 2.5, lon: 0.7, ramp: 0.3, label: 'Floor it' },
      { dur: 2.5, lon: 0.45, label: 'Still pulling' },
      { dur: 4, lon: 0.05, label: 'Cruise' },
      { dur: 3, lon: -0.2, label: 'Slow down' },
      { dur: 3, label: 'Stopped' },
    ],
  },
  {
    id: 'brake', name: 'Hard brake at a light',
    about: 'Cruising, then a sudden stop for a red light.',
    steps: [
      { dur: 9, label: 'Parked' },
      { dur: 4, lon: 0.2, label: 'Get going' },
      { dur: 3, label: 'Cruise' },
      { dur: 2.2, lon: -0.65, ramp: 0.25, label: 'Slam the brakes' },
      { dur: 5, label: 'Stopped at the light' },
    ],
  },
  {
    id: 'lane', name: 'Quick lane change',
    about: 'A brisk left then right on the highway.',
    steps: [
      { dur: 9, label: 'Parked' },
      { dur: 4, lon: 0.25, label: 'Get going' },
      { dur: 2, label: 'Cruise' },
      { dur: 1.2, lat: -0.4, ramp: 0.3, label: 'Swerve left' },
      { dur: 1.2, lat: 0.45, ramp: 0.4, label: 'Back right' },
      { dur: 4, label: 'Cruise' },
    ],
  },
  {
    id: 'track', name: 'Track day corner',
    about: 'A long, fast right-hander at 0.8 g, then a hard left.',
    steps: [
      { dur: 9, label: 'Parked' },
      { dur: 4, lon: 0.4, label: 'Accelerate' },
      { dur: 1.5, lon: -0.5, label: 'Brake for the corner' },
      { dur: 5, lat: 0.8, ramp: 0.6, label: 'Hold the right-hander' },
      { dur: 1, label: 'Straighten' },
      { dur: 3, lat: -0.75, ramp: 0.5, label: 'Hard left' },
      { dur: 4, label: 'Cruise' },
    ],
  },
  {
    id: 'roundabout', name: 'Roundabout',
    about: 'Slow in, right to enter, long left around, right to exit.',
    steps: [
      { dur: 9, label: 'Parked' },
      { dur: 4, lon: 0.2, label: 'Get going' },
      { dur: 2, lon: -0.25, label: 'Slow for the roundabout' },
      { dur: 1, lat: 0.3, label: 'Enter (right)' },
      { dur: 4, lat: -0.45, label: 'Go around (left)' },
      { dur: 1, lat: 0.3, label: 'Exit (right)' },
      { dur: 3, lon: 0.3, label: 'Pull away' },
      { dur: 3, label: 'Cruise' },
    ],
  },
  {
    id: 'bumpy', name: 'Potholes and cobbles',
    about: 'Straight and steady: smooth tarmac, two potholes, then a cobbled street. No turns or hard braking, so only the road should get a reaction.',
    settings: { road: 0.4 },
    steps: [
      { dur: 9, label: 'Parked' },
      { dur: 3, lon: 0.15, label: 'Get going' },
      { dur: 3, label: 'Smooth tarmac' },
      { dur: 2.5, label: 'Pothole!', hit: 1 },
      { dur: 2.5, label: 'Another pothole!', hit: -1 },
      { dur: 8, label: 'Cobbled street', cobbles: 1 },
      { dur: 4, label: 'Smooth again' },
      { dur: 3, lon: -0.15, label: 'Easy stop' },
    ],
  },
  {
    id: 'hill', name: 'Started on a hill',
    about: 'Switched on while parked facing up a 10° slope, gentle drive up, then a gentle stop on flat road. Tests the startup calibration.',
    steps: [
      { dur: 9, label: 'Parked uphill', grade: 10 },
      { dur: 4, lon: 0.15, grade: 10, label: 'Gentle drive up the hill' },
      { dur: 3, grade: 0, ramp: 2, label: 'Crest onto flat road' },
      { dur: 4, label: 'Cruise on the flat' },
      { dur: 3, lon: -0.2, label: 'Gentle stop' },
      { dur: 3, label: 'Stopped' },
    ],
  },
  {
    id: 'tilted', name: 'Tilted mount',
    about: 'The companion leans back 20° on the dash, on a calm drive.',
    settings: { mountPitch: 20 },
    steps: [
      { dur: 9, label: 'Parked' },
      { dur: 4, lon: 0.15, label: 'Pull away' },
      { dur: 3, lat: 0.2, label: 'Soft right turn' },
      { dur: 5, label: 'Cruise' },
    ],
  },
];

export function scenarioDuration(sc) {
  return sc.steps.reduce((a, s) => a + s.dur, 0);
}

// Sample a scripted drive at time t. Returns { lon, lat, grade, potholes, label, step }.
export function scenarioAt(sc, t) {
  const s0 = sc.steps[0];
  let start = 0, prev = { lon: s0.lon || 0, lat: s0.lat || 0, grade: s0.grade || 0 };
  for (let i = 0; i < sc.steps.length; i++) {
    const s = sc.steps[i];
    const target = { lon: s.lon || 0, lat: s.lat || 0, grade: s.grade ?? prev.grade };
    if (t < start + s.dur || i === sc.steps.length - 1) {
      const ramp = s.ramp ?? 0.5;
      const k = smooth(Math.min(1, Math.max(0, (t - start) / ramp)));
      return {
        lon: prev.lon + (target.lon - prev.lon) * k,
        lat: prev.lat + (target.lat - prev.lat) * k,
        grade: prev.grade + (target.grade - prev.grade) * k,
        potholes: s.potholes || 0,
        cobbles: s.cobbles || 0,
        hit: s.hit || 0,
        label: t >= start + s.dur ? 'Done' : s.label,
        step: i,
        done: t >= start + s.dur,
      };
    }
    prev = target;
    start += s.dur;
  }
}

function smooth(k) { return k * k * (3 - 2 * k); }

// A small arcade-ish car for keyboard / touch driving.
export const CARS = {
  family: { name: 'Family car', powerW: 55000, massKg: 1400, maxG: 0.5, gasRate: 1.6 },
  sporty: { name: 'Sporty', powerW: 220000, massKg: 1500, maxG: 0.85, gasRate: 3 },
};

export class Car {
  constructor(spec = CARS.family) { this.spec = spec; this.reset(); }
  reset() {
    this.v = 0;          // m/s
    this.throttle = 0; this.brake = 0; this.steer = 0;
    this.lon = 0; this.lat = 0;
    this.heading = 0; this.x = 0; this.y = 0;
    this.odometer = 0;
  }
  // inputs: { up, down, left, right } booleans (or analog 0..1)
  step(dt, input) {
    const approach = (cur, target, rate) => cur + Math.max(-rate * dt, Math.min(rate * dt, target - cur));
    this.throttle = approach(this.throttle, +input.up, input.up ? this.spec.gasRate : 4);
    this.brake = approach(this.brake, +input.down, input.down ? 2.0 : 5);
    const steerTarget = (+input.right) - (+input.left);
    this.steer = approach(this.steer, steerTarget, steerTarget ? 1.4 : 2.5);
    const g = 9.81;
    const vSafe = Math.max(this.v, 1);
    const { powerW, massKg, maxG } = this.spec;
    const drive = Math.min(maxG * g, powerW / (massKg * vSafe)) * this.throttle;
    const brake = this.v > 0.05 ? 0.95 * g * this.brake : 0;
    const drag = 0.0004 * this.v * this.v + (this.v > 0.05 ? 0.12 : 0);
    let a = drive - brake - drag;
    if (this.v <= 0.05 && a < 0) a = 0;
    // Lateral: the faster you go, the less steering lock you get.
    const maxCurv = 1 / (5 + 0.05 * this.v * this.v);
    let lat = this.v * this.v * maxCurv * this.steer;
    // Friction circle (about 1 g of grip).
    const grip = 0.98 * g, tot = Math.hypot(a, lat);
    if (tot > grip) { a *= grip / tot; lat *= grip / tot; }
    this.v = Math.max(0, this.v + a * dt);
    this.lon = a / g; this.lat = lat / g;
    if (this.v > 0.1) this.heading += (lat / this.v) * dt;
    this.x += Math.sin(this.heading) * this.v * dt;
    this.y += Math.cos(this.heading) * this.v * dt;
    this.odometer += this.v * dt;
  }
}

// Deterministic noise so a scripted run gives the same result every time.
export class Rng {
  constructor(seed = 12345) { this.s = seed >>> 0 || 1; }
  next() { let s = this.s; s ^= s << 13; s ^= s >>> 17; s ^= s << 5; this.s = s >>> 0; return this.s / 4294967296; }
  gauss() { const u = Math.max(1e-9, this.next()), v = this.next(); return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v); }
}

// Turns what the car is doing into what the accelerometer reads.
export class Sensor {
  constructor(settings = {}, seed = 1) {
    this.settings = { ...DEFAULT_SETTINGS, ...settings };
    this.rng = new Rng(seed);
    this.bump = 0; this.bumpVel = 0;   // suspension "spring" for potholes
    this.lastT = 0;
    this.pitchDyn = 0; this.rollDyn = 0;
  }

  // motion: { lon, lat, grade (deg, + nose up), speed (m/s), potholes (per second) }
  read(t, motion) {
    const s = this.settings;
    const dt = Math.min(0.1, Math.max(0, t - this.lastT));
    this.lastT = t;
    const moving = (motion.speed ?? 10) > 0.3;

    // Body motion: nose dives ~1.5°/g under braking, rolls ~3°/g in turns (lags a little).
    const k = 1 - Math.exp(-dt / 0.15);
    const pTarget = s.bodyMotion ? 1.5 * motion.lon : 0;
    const rTarget = s.bodyMotion ? 3 * motion.lat : 0;
    this.pitchDyn += (pTarget - this.pitchDyn) * k;
    this.rollDyn += (rTarget - this.rollDyn) * k;

    // Potholes and road texture: a damped spring kicked at random, stepped
    // in 1 ms slices (the sketch may read the sensor seconds apart).
    const rate = (motion.potholes || 0) + (moving ? s.road * 0.3 : 0);
    // Cobbles: a steady stream of small kicks. The road slider adds them above 0.6.
    const cobbles = moving ? Math.max(motion.cobbles || 0, Math.min(1, Math.max(0, (s.road - 0.6) / 0.4))) : 0;
    // One-off hits from a scripted drive (a pothole at the start of a step).
    if (motion.hit && motion.step !== this.hitStep) { this.bumpVel += motion.hit * 45; }
    this.hitStep = motion.step;
    const w = 2 * Math.PI * 8, z = 0.25;
    const span = Math.min(dt, 0.3), steps = Math.ceil(span / 0.001);
    if (dt > span) { this.bump *= Math.exp(-z * w * (dt - span)); this.bumpVel *= Math.exp(-z * w * (dt - span)); }
    for (let i = 0; i < steps; i++) {
      const h = span / steps;
      if (this.rng.next() < rate * h) this.bumpVel += (this.rng.next() < 0.5 ? -1 : 1) * (0.6 + this.rng.next()) * 25 * Math.max(0.3, s.road);
      if (this.rng.next() < cobbles * 14 * h) this.bumpVel += (this.rng.next() < 0.5 ? -1 : 1) * (0.5 + this.rng.next()) * 9 * cobbles;
      this.bumpVel += (-w * w * this.bump - 2 * z * w * this.bumpVel) * h;
      this.bump += this.bumpVel * h;
    }
    const bumpG = -w * w * this.bump / 9.81 * 0.01;

    // Specific force in the car frame (forward, right, up), in g.
    const pitch = (motion.grade || 0) * DEG; // + nose up
    let f = [
      motion.lon + Math.sin(pitch),
      motion.lat,
      Math.cos(pitch) + bumpG,
    ];
    // Body pitch/roll relative to the road tilt gravity into the axes.
    f = rotY(f, (-this.pitchDyn) * DEG);
    f = rotX(f, (this.rollDyn) * DEG);
    // Mounting on the dash.
    f = rotY(f, -s.mountPitch * DEG);
    f = rotX(f, s.mountRoll * DEG);
    f = rotZ(f, s.mountYaw * DEG);

    const vib = moving ? s.road * 0.03 : 0;
    const n = () => this.rng.gauss() * s.noise + this.rng.gauss() * vib;
    let x = -f[0] + n() + bumpG * 0.2;  // sensor X points backward
    let y = f[1] + n();
    let zz = f[2] + n() * 1.5;
    if (s.flipX) x = -x;
    if (s.flipY) y = -y;
    return [x, y, zz];
  }
}

// Rotations in the (forward, right, up) frame.
function rotY(v, a) { // pitch: about the right axis; + tips forward axis upward
  const c = Math.cos(a), s = Math.sin(a);
  return [v[0] * c - v[2] * s, v[1], v[0] * s + v[2] * c];
}
function rotX(v, a) { // roll: about the forward axis
  const c = Math.cos(a), s = Math.sin(a);
  return [v[0], v[1] * c + v[2] * s, -v[1] * s + v[2] * c];
}
function rotZ(v, a) { // yaw: about the up axis
  const c = Math.cos(a), s = Math.sin(a);
  return [v[0] * c + v[1] * s, -v[0] * s + v[1] * c, v[2]];
}
