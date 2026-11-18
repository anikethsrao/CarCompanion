/*
Car Companion V0.7 (Seeed XIAO ESP32)

V0.7: works out at power-on which sensor axis is up and which is forward (new
enclosure), turns flipped to match, gentle lean when speeding up or braking,
rough road less twitchy and tunable, cruising acts while driving, and faster
stroke drawing so TURBO and WIPERS don't lag.

V0.6: rough roads. The vertical axis is read too; bumps shake the face in
sync with the road and a rough stretch makes it RATTLED (gritted teeth).

Procedural face (CarFace.h) instead of bitmaps. Non-blocking: never delay() in loop.
Sensor: ADXL345 on the GY-85. Level calibration at startup (keep the car still),
then smoothed, gravity-removed g goes to face.drive().
*/

#include <Wire.h>
#include <ADXL345.h>
#include <Adafruit_SSD1306.h>
#include "CarFace.h"

//Make Sure Address is Correct
#define OLED_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
//Bob's your uncle, this line fixes the Seeeduino Xiao
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
carface::CarFace<Adafruit_SSD1306> face(display);
ADXL345 adxl;

// Axis mapping. 0 = X, 1 = Y, 2 = Z on the accelerometer.
// At power-on the sketch finds which axis gravity is on (that one is "up").
// Sideways is LAT_AXIS, and forward is whichever axis is left over, so it
// keeps working when the sensor is mounted flat, upright or on its side.
// If a direction comes out backwards, flip its SIGN (1 or -1).
// Check with the serial log: speeding up -> "forward g" positive,
// turning right -> "lateral g" positive.
const int LAT_AXIS = 1;
const float LAT_SIGN = -1;   // new enclosure: turns were mirrored with 1
const float FWD_SIGN = -1;   // flip if braking tips the face back instead of forward
int fwdAxis = 0, upAxis = 2; // found in calibrate()

// Smoothing: exponential moving average, 0..1 (lower = smoother, slower)
const float SMOOTHING = 0.3;
const int CAL_SAMPLES = 50;

// A brisk pull-away is about 0.25 g; only flash SHOCKED above this.
const float SHOCK_G = 0.35;

// Slope tracking. Startup calibration can't tell a tilted mount from a sloped
// road (switched on parked on a hill), so the baseline keeps drifting toward
// the reading while the car is driving gently. Hard events (above
// ADAPT_MAX_G) never move it. Slopes fade out over a few seconds.
const float ADAPT_TAU_S = 4.0;
const float ADAPT_MAX_G = 0.3;
const uint16_t SAMPLE_MS = 20;   // read the sensor at 50 Hz
const uint16_t LOG_MS = 200;     // serial log rate

// Road feel. A slow average of the vertical axis tracks gravity; what's left
// is the jolt from each bump. Roughness is a ~0.5 s average of jolt size.
const float UP_TRACK = 0.05;     // how fast the gravity estimate follows
const float ROUGH_SMOOTHING = 0.04;
// Watch "rough g" in the serial log on a normal road and on a bad one, and
// set these between the two.
const float ROUGH_ON_G = 0.20;   // RATTLED face starts above this...
const float ROUGH_HOLD_S = 1.5;  // ...once it has lasted this long
const float ROUGH_OFF_G = 0.14;  // and ends below this
const float BUMP_MIN_G = 0.08;   // single jolts smaller than this don't shake the face

// Cruising acts (whistling, wipers, turbo...) while driving steadily: one
// every CRUISE_GAP_S to 2x CRUISE_GAP_S seconds. Bigger = rarer.
const float CRUISE_GAP_S = 45;

double xyz[3];
float fwdBase = 0, latBase = 0;
float forwardG = 0, lateralG = 0;
float upBase = 1, joltG = 0, roughG = 0;
uint32_t lastSample = 0, lastLog = 0;

float upG = 1;  // last vertical reading

void readAxes(float &fwd, float &lat) {
  adxl.getAcceleration(xyz);
  fwd = FWD_SIGN * xyz[fwdAxis];
  lat = LAT_SIGN * xyz[LAT_AXIS];
  upG = xyz[upAxis];
}

void calibrate() {
  // Average every axis with the car still.
  float avg[3] = { 0, 0, 0 };
  for (int i = 0; i < CAL_SAMPLES; i++) {
    adxl.getAcceleration(xyz);
    for (int a = 0; a < 3; a++) avg[a] += xyz[a] / CAL_SAMPLES;
    delay(10);
  }
  // Gravity (about 1 g) shows which axis points up. Sideways is fixed, so
  // forward is the one left over.
  upAxis = -1;
  for (int a = 0; a < 3; a++)
    if (a != LAT_AXIS && (upAxis < 0 || fabs(avg[a]) > fabs(avg[upAxis]))) upAxis = a;
  fwdAxis = 3 - LAT_AXIS - upAxis;

  upG = upBase = avg[upAxis];
  fwdBase = FWD_SIGN * avg[fwdAxis];
  latBase = LAT_SIGN * avg[LAT_AXIS];
  const char AXIS_NAME[3] = { 'X', 'Y', 'Z' };
  Serial.print("Axes: up ");
  Serial.print(AXIS_NAME[upAxis]);
  Serial.print(", forward ");
  Serial.print(AXIS_NAME[fwdAxis]);
  Serial.print(", sideways ");
  Serial.println(AXIS_NAME[LAT_AXIS]);
  Serial.print("Calibrated forward base: ");
  Serial.print(fwdBase);
  Serial.print("  lateral base: ");
  Serial.println(latBase);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);   // faster I2C = smoother frames
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  display.clearDisplay();
  display.display();
  adxl.powerOn();

  calibrate();   // car must be still here

  face.begin(millis(), analogRead(A0) * 2654435761u + 1);   // seed blinks/glances
  face.setShockThreshold(SHOCK_G);
  face.setRoughThresholds(ROUGH_ON_G, ROUGH_OFF_G, BUMP_MIN_G, ROUGH_HOLD_S);
  face.setCruiseGap(CRUISE_GAP_S);
}

void loop() {
  uint32_t now = millis();

  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    float f, l;
    readAxes(f, l);
    forwardG += SMOOTHING * ((f - fwdBase) - forwardG);
    lateralG += SMOOTHING * ((l - latBase) - lateralG);

    // Road feel from the vertical axis.
    joltG = upG - upBase;
    upBase += UP_TRACK * joltG;
    roughG += ROUGH_SMOOTHING * (fabs(joltG) - roughG);
    face.road(joltG, roughG, now);

    // Track slow changes (road slope, camber) into the baseline.
    float k = (SAMPLE_MS / 1000.0) / ADAPT_TAU_S;
    if (fabs(forwardG) < ADAPT_MAX_G && fabs(lateralG) < ADAPT_MAX_G) {
      fwdBase += k * forwardG;
      latBase += k * lateralG;
    }
  }

  if (now - lastLog >= LOG_MS) {
    lastLog = now;
    Serial.print("forward g: ");
    Serial.print(forwardG);
    Serial.print("   lateral g: ");
    Serial.print(lateralG);
    Serial.print("   rough g: ");
    Serial.println(roughG);
  }

  face.drive(forwardG, lateralG, now);
  face.update(now);
}
