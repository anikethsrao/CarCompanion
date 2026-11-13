/*
Car Companion V0.5 (Seeed XIAO)

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

// Axis mapping: which accelerometer axis points along the car, and which way.
// 0 = X, 1 = Y, 2 = Z. Flip a SIGN to -1 if that direction comes out backwards.
// Check with the serial log: speeding up -> forward positive,
// turning right -> lateral positive.
const int FWD_AXIS = 0;
const float FWD_SIGN = -1;   // old mounting: X went negative when speeding up
const int LAT_AXIS = 1;
const float LAT_SIGN = 1;

// Smoothing: exponential moving average, 0..1 (lower = smoother, slower)
const float SMOOTHING = 0.3;
const int CAL_SAMPLES = 50;
const uint16_t SAMPLE_MS = 20;   // read the sensor at 50 Hz
const uint16_t LOG_MS = 200;     // serial log rate

double xyz[3];
float fwdBase = 0, latBase = 0;
float forwardG = 0, lateralG = 0;
uint32_t lastSample = 0, lastLog = 0;

void readAxes(float &fwd, float &lat) {
  adxl.getAcceleration(xyz);
  fwd = FWD_SIGN * xyz[FWD_AXIS];
  lat = LAT_SIGN * xyz[LAT_AXIS];
}

void calibrate() {
  float sf = 0, sl = 0, f, l;
  for (int i = 0; i < CAL_SAMPLES; i++) {
    readAxes(f, l);
    sf += f;
    sl += l;
    delay(10);
  }
  fwdBase = sf / CAL_SAMPLES;
  latBase = sl / CAL_SAMPLES;
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
}

void loop() {
  uint32_t now = millis();

  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    float f, l;
    readAxes(f, l);
    forwardG += SMOOTHING * ((f - fwdBase) - forwardG);
    lateralG += SMOOTHING * ((l - latBase) - lateralG);
  }

  if (now - lastLog >= LOG_MS) {
    lastLog = now;
    Serial.print("forward g: ");
    Serial.print(forwardG);
    Serial.print("   lateral g: ");
    Serial.println(lateralG);
  }

  face.drive(forwardG, lateralG, now);
  face.update(now);
}
