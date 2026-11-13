// Runtime glue between the sketch and the simulator (JavaScript side).
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

extern "C" {
SIM_IMPORT(sim_display) void sim_display(const uint8_t *buf, int w, int h, int inverted, int on, int contrast, double transfer_us);
SIM_IMPORT(sim_accel_sample) void sim_accel_sample(double *xyz_g);
SIM_IMPORT(sim_event) void sim_event(const char *name, int len, int value);
SIM_IMPORT(sim_register_bitmap) void sim_register_bitmap(const char *name, int len, const uint8_t *ptr, int size);
}

HardwareSerial Serial;
TwoWire Wire;
SPIClass SPI;

// ---------- time ----------
unsigned long millis() { return (unsigned long)(sim_now() / 1000.0); }
unsigned long micros() { return (unsigned long)sim_now(); }
void delay(unsigned long ms) { sim_delay((double)ms); }
void delayMicroseconds(unsigned int us) { sim_delay(us / 1000.0); }
void yield() { sim_delay(0); }

// ---------- pins ----------
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t pin, uint8_t val) { sim_digital_write(pin, val); }
int digitalRead(uint8_t pin) { return sim_digital_read(pin); }
int analogRead(uint8_t pin) { return sim_analog_read(pin); }
void analogWrite(uint8_t pin, int val) { sim_digital_write(pin, val); }

static uint32_t rng_state = 1;
static bool rng_seeded = false;
static uint32_t rng_next() {
  if (!rng_seeded) { rng_state = (uint32_t)(sim_random() * 4294967295.0) | 1; rng_seeded = true; }
  rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5;
  return rng_state;
}
long random(long howbig) { return howbig <= 0 ? 0 : (long)(rng_next() % (uint32_t)howbig); }
long random(long howsmall, long howbig) { return howsmall >= howbig ? howsmall : howsmall + random(howbig - howsmall); }
void randomSeed(unsigned long seed) { if (seed) { rng_state = seed; rng_seeded = true; } }
long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ---------- Print ----------
size_t Print::write(const uint8_t *buffer, size_t size) {
  size_t n = 0;
  while (size--) { if (write(*buffer++)) n++; else break; }
  return n;
}

size_t Print::print(unsigned long n, int base) {
  char buf[8 * sizeof(long) + 1];
  char *str = &buf[sizeof(buf) - 1];
  *str = '\0';
  if (base < 2) base = 10;
  do { char c = n % base; n /= base; *--str = c < 10 ? c + '0' : c + 'A' - 10; } while (n);
  return write(str);
}

size_t Print::print(long n, int base) {
  if (base == 10 && n < 0) { size_t t = print('-'); return t + print((unsigned long)(-n), 10); }
  return print((unsigned long)n, base);
}

// Same algorithm as Arduino's Print::printFloat, so Serial output matches.
size_t Print::print(double number, int digits) {
  size_t n = 0;
  if (isnan(number)) return print("nan");
  if (isinf(number)) return print("inf");
  if (number > 4294967040.0) return print("ovf");
  if (number < -4294967040.0) return print("ovf");
  if (number < 0.0) { n += print('-'); number = -number; }
  double rounding = 0.5;
  for (uint8_t i = 0; i < digits; ++i) rounding /= 10.0;
  number += rounding;
  unsigned long int_part = (unsigned long)number;
  double remainder = number - (double)int_part;
  n += print(int_part);
  if (digits > 0) n += print('.');
  while (digits-- > 0) {
    remainder *= 10.0;
    unsigned int toPrint = (unsigned int)remainder;
    n += print(toPrint);
    remainder -= toPrint;
  }
  return n;
}

size_t Print::printf(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  int len = vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  if (len < 0) return 0;
  return write((const uint8_t *)buf, len < (int)sizeof buf ? len : (int)sizeof buf - 1);
}

void HardwareSerial::begin(unsigned long baud) { baud_ = baud; sim_event("serial-begin", 12, (int)baud); }
size_t HardwareSerial::write(uint8_t c) { sim_serial_write((const char *)&c, 1); return 1; }
size_t HardwareSerial::write(const uint8_t *buffer, size_t size) { sim_serial_write((const char *)buffer, (int)size); return size; }

// ---------- ADXL345 register model ----------
static uint8_t adxl_regs[64];
static bool adxl_init = false;
static void adxl_reset() {
  if (adxl_init) return;
  adxl_init = true;
  memset(adxl_regs, 0, sizeof adxl_regs);
  adxl_regs[0x00] = 0xE5;  // DEVID
  adxl_regs[0x2C] = 0x0A;  // BW_RATE 100 Hz
  adxl_regs[0x30] = 0x02;  // INT_SOURCE: watermark
}

static void adxl_sample() {
  // Only measures when POWER_CTL.Measure is set, like the real chip.
  if (!(adxl_regs[0x2D] & 0x08)) return;
  double g[3];
  sim_accel_sample(g);
  uint8_t fmt = adxl_regs[0x31];
  int range = 2 << (fmt & 3);
  bool fullRes = fmt & 0x08;
  double lsb = fullRes ? 0.0039 : (2.0 * range / 1024.0);
  int maxRaw = fullRes ? range * 256 : 512;
  for (int k = 0; k < 3; k++) {
    double v = g[k] / lsb;
    long raw = (long)(v < 0 ? v - 0.5 : v + 0.5);
    if (raw > maxRaw - 1) raw = maxRaw - 1;
    if (raw < -maxRaw) raw = -maxRaw;
    int16_t r = (int16_t)raw;
    adxl_regs[0x32 + 2 * k] = r & 0xFF;
    adxl_regs[0x33 + 2 * k] = (r >> 8) & 0xFF;
  }
}

uint8_t sim_adxl_read_reg(uint8_t reg) {
  adxl_reset();
  reg &= 0x3F;
  if (reg == 0x32) adxl_sample();  // a burst read always starts at DATAX0
  return adxl_regs[reg];
}

void sim_adxl_write_reg(uint8_t reg, uint8_t val) {
  adxl_reset();
  reg &= 0x3F;
  if (reg >= 0x1D && reg != 0x30 && !(reg >= 0x32 && reg <= 0x39)) adxl_regs[reg] = val;
}

bool sim_i2c_device_present(uint8_t a) { return a == 0x53 || a == 0x3C || a == 0x3D; }

// ---------- Wire ----------
void TwoWire::beginTransmission(uint8_t address) { txAddr_ = address; txLen_ = 0; }
size_t TwoWire::write(uint8_t data) { if (txLen_ < (int)sizeof txBuf_) txBuf_[txLen_++] = data; return 1; }
size_t TwoWire::write(const uint8_t *data, size_t n) { for (size_t i = 0; i < n; i++) write(data[i]); return n; }
uint8_t TwoWire::endTransmission(bool) {
  if (!sim_i2c_device_present(txAddr_)) return 2;  // NACK on address
  if (txAddr_ == 0x53 && txLen_ > 0) {
    regPtr_ = txBuf_[0];
    for (int i = 1; i < txLen_; i++) sim_adxl_write_reg(regPtr_ + i - 1, txBuf_[i]);
  }
  txLen_ = 0;
  return 0;
}
uint8_t TwoWire::requestFrom(uint8_t address, size_t quantity, bool) {
  rxLen_ = rxPos_ = 0;
  if (!sim_i2c_device_present(address)) return 0;
  if (quantity > sizeof rxBuf_) quantity = sizeof rxBuf_;
  for (size_t i = 0; i < quantity; i++) rxBuf_[rxLen_++] = address == 0x53 ? sim_adxl_read_reg(regPtr_ + i) : 0;
  return rxLen_;
}

// ---------- SSD1306 ----------
Adafruit_SSD1306::Adafruit_SSD1306(uint8_t w, uint8_t h, TwoWire *, int8_t, uint32_t clkDuring, uint32_t)
    : Adafruit_GFX(w, h), clkDuring_(clkDuring) { memset(buffer, 0, sizeof buffer); }
Adafruit_SSD1306::Adafruit_SSD1306(uint8_t w, uint8_t h, int8_t, int8_t, int8_t, int8_t, int8_t)
    : Adafruit_GFX(w, h), clkDuring_(8000000UL) { memset(buffer, 0, sizeof buffer); }
Adafruit_SSD1306::Adafruit_SSD1306(uint8_t w, uint8_t h, SPIClass *, int8_t, int8_t, int8_t, uint32_t bitrate)
    : Adafruit_GFX(w, h), clkDuring_(bitrate) { memset(buffer, 0, sizeof buffer); }
Adafruit_SSD1306::Adafruit_SSD1306(int8_t) : Adafruit_GFX(128, 64) { memset(buffer, 0, sizeof buffer); }

bool Adafruit_SSD1306::begin(uint8_t, uint8_t addr, bool, bool) {
  i2caddr_ = addr ? addr : (HEIGHT == 32 ? 0x3C : 0x3D);
  begun_ = true;
  clearDisplay();
  sim_event("begin", 5, i2caddr_);
  return true;
}

void Adafruit_SSD1306::display() {
  // Frame + command bytes over I2C, about 9 bits per byte on the wire.
  double bytes = WIDTH * ((HEIGHT + 7) / 8) + 8;
  double us = bytes * 9.0 * 1e6 / (double)(clkDuring_ ? clkDuring_ : 400000);
  if (!begun_) { sim_event("display-before-begin", 20, 0); return; }
  sim_display(buffer, WIDTH, HEIGHT, inverted_, on_, contrast_, us);
}

void Adafruit_SSD1306::clearDisplay() {
  memset(buffer, 0, WIDTH * ((HEIGHT + 7) / 8));
  sim_event("clear", 5, 0);
}

void Adafruit_SSD1306::invertDisplay(bool i) { inverted_ = i; }
void Adafruit_SSD1306::dim(bool d) { contrast_ = d ? 0 : 0xCF; }

void Adafruit_SSD1306::ssd1306_command(uint8_t c) {
  if (pendingCmd_ == SSD1306_SETCONTRAST) { contrast_ = c; pendingCmd_ = 0; return; }
  switch (c) {
    case SSD1306_DISPLAYOFF: on_ = false; break;
    case SSD1306_DISPLAYON: on_ = true; break;
    case SSD1306_NORMALDISPLAY: inverted_ = false; break;
    case SSD1306_INVERTDISPLAY: inverted_ = true; break;
    case SSD1306_SETCONTRAST: pendingCmd_ = c; break;
  }
}

void Adafruit_SSD1306::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x >= 0) && (x < width()) && (y >= 0) && (y < height())) {
    switch (getRotation()) {
      case 1: { int16_t t = x; x = WIDTH - y - 1; y = t; } break;
      case 2: x = WIDTH - x - 1; y = HEIGHT - y - 1; break;
      case 3: { int16_t t = x; x = y; y = HEIGHT - t - 1; } break;
    }
    uint8_t *p = &buffer[x + (y / 8) * WIDTH];
    switch (color) {
      case SSD1306_WHITE: *p |= (1 << (y & 7)); break;
      case SSD1306_BLACK: *p &= ~(1 << (y & 7)); break;
      case SSD1306_INVERSE: *p ^= (1 << (y & 7)); break;
    }
  }
}

void Adafruit_SSD1306::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  for (int16_t i = 0; i < w; i++) drawPixel(x + i, y, color);
}
void Adafruit_SSD1306::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  for (int16_t i = 0; i < h; i++) drawPixel(x, y + i, color);
}

bool Adafruit_SSD1306::getPixel(int16_t x, int16_t y) {
  if ((x >= 0) && (x < width()) && (y >= 0) && (y < height())) {
    switch (getRotation()) {
      case 1: { int16_t t = x; x = WIDTH - y - 1; y = t; } break;
      case 2: x = WIDTH - x - 1; y = HEIGHT - y - 1; break;
      case 3: { int16_t t = x; x = y; y = HEIGHT - t - 1; } break;
    }
    return buffer[x + (y / 8) * WIDTH] & (1 << (y & 7));
  }
  return false;
}

// ---------- entry points ----------
static char asyncify_data[64 * 1024] __attribute__((aligned(8)));

extern "C" {
__attribute__((export_name("sim_asyncify_data"))) void *sim_asyncify_data() { return asyncify_data; }
__attribute__((export_name("sim_asyncify_size"))) int sim_asyncify_size() { return sizeof asyncify_data; }
__attribute__((export_name("sim_setup"))) void sim_setup() { setup(); }
__attribute__((export_name("sim_loop"))) void sim_loop() { loop(); }
__attribute__((export_name("sim_malloc"))) void *sim_malloc(int n) { return malloc(n); }
}

// Generated per sketch by the build: registers each PROGMEM bitmap by name.
extern "C" void sim_register_sketch_bitmaps();
extern "C" __attribute__((export_name("sim_register"))) void sim_register() { sim_register_sketch_bitmaps(); }
extern "C" void sim_register_bitmap_c(const char *name, const uint8_t *ptr, int size) {
  sim_register_bitmap(name, (int)strlen(name), ptr, size);
}
