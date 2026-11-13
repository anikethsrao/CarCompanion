# Car Companion simulator

Test the companion without the car or the Arduino. The simulator compiles the
real sketch (`firmware/CarCompanion/CarCompanion.ino`) to WebAssembly and runs
it against a simulated car and accelerometer, drawing the 128x64 OLED in your
browser.

## Quick start

You need [Node.js](https://nodejs.org) 20 or newer. Nothing else: the C++
compiler is an npm package.

```sh
cd simulator
npm install
npm run sim -- ../firmware/CarCompanion/CarCompanion.ino
```

That opens http://localhost:8137. Leave it running: every time you save the
sketch (in the Arduino IDE or anywhere else) it recompiles and the page
restarts the companion with the new code. Compile errors show on the screen.

## What you can do on the page

- **Scripted drives**: calm city driving, a hard launch, a hard brake, a lane
  change, a track corner, a roundabout, a bumpy road, starting up on a hill
  and a tilted mount. Each starts parked so the wake-up animation and the
  startup calibration happen with the car still.
- **Drive it**: arrow keys or WASD (hold-to-press buttons on a phone). A small
  car model turns gas, brake and steering into g-forces.
- **g-force pad**: drag a dot to set forward and sideways g directly. The
  dashed square is the sketch's 0.3 g trigger.
- **Sensor and mounting**: lean or twist the companion on the dash, flip an
  axis, change sensor noise and road roughness.
- Speed from 0.25x to 8x, pause, and power cycle.
- A 30-second trace of sensor X and Y, with dots where the sketch actually
  read the sensor, and a strip of which face was on screen.
- The serial monitor.

## Checking a change without a browser

```sh
node tools/run.mjs ../firmware/CarCompanion/CarCompanion.ino              # all drives
node tools/run.mjs ../firmware/CarCompanion/CarCompanion.ino --drive lane # one drive
node tools/run.mjs ../firmware/CarCompanion/CarCompanion.ino --serial     # with Serial output
```

It prints which face was on screen when, next to what the car was doing. A
GitHub Action runs this on every push that touches the firmware.

## Sharing a build

```sh
node tools/bundle.mjs -o dist/simulator.html "Mine=../firmware/CarCompanion/CarCompanion.ino"
```

writes one HTML file with the simulator and that build inside, which opens
from disk. Several `Label=path` pairs put several builds in the firmware menu.

## How faithful is it?

The sketch's own code runs unchanged: your `if`s, thresholds, bitmaps, bugs
and timing. Things that are stand-ins:

| Real | In the simulator |
| --- | --- |
| Arduino core (millis, delay, Serial, String) | `core/Arduino.h`, `core/sim_runtime.cpp`. Time is virtual, so `delay(2500)` blocks the sketch for 2.5 simulated seconds exactly like on the XIAO. Serial number formatting matches Arduino's. |
| Seeed ADXL345 library | `core/ADXL345.h`, same API, talking to a simulated ADXL345 register file over a simulated `Wire`. Default range is +/-2 g and readings clip there. The library's fixed axis gains are kept, so Z at rest reads about 0.89 g, like the real part. Forgetting `powerOn()` gives zeros. |
| Adafruit SSD1306 / GFX | `core/Adafruit_SSD1306.h`, `core/Adafruit_GFX.*`, same drawing calls and algorithms. `display()` costs the ~25 ms an I2C frame transfer takes. Text uses a built-in 5x7 font that is close to, but not byte-identical with, Adafruit's. |
| The car | `web/world.js`. Sensor axes as the firmware expects: X points to the back of the car (accelerating reads X < 0), Y to the right, Z up. Includes body dive and roll, potholes and road vibration, hills and mount tilt. |

The sensor in the simulator is perfectly calibrated apart from the library
gains, so real-car numbers will be noisier. If the real companion reacts
differently, the mounting settings (lean, twist, axis flips) are the first
thing to match.

## Procedural faces

Sketches that draw with `CarFace.h` (V0.5 onward) have no bitmaps to name, so
the build asks the face object for its mood, and the page and `run.mjs` show
names like `braking` or `content, leaning left`.

## Adding things

- New drive: add an entry to `SCENARIOS` in `web/world.js`.
- Another library: add a header to `core/`. Headers and `.cpp` files next to
  the sketch are compiled with it, like in the Arduino IDE.
