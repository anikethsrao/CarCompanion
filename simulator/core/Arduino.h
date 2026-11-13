// Car Companion simulator: a small stand-in for the Arduino core.
// Only what the sketch (and typical sketches) need. Time, sensors and the
// screen are handled by the JavaScript side of the simulator.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#define SIM_IMPORT(name) __attribute__((import_module("env"), import_name(#name)))

extern "C" {
// Provided by the simulator (JavaScript). delay() pauses the sketch.
SIM_IMPORT(sim_delay) void sim_delay(double ms);
SIM_IMPORT(sim_now) double sim_now();  // virtual time in microseconds
SIM_IMPORT(sim_serial_write) void sim_serial_write(const char *data, int len);
SIM_IMPORT(sim_random) double sim_random();
SIM_IMPORT(sim_analog_read) int sim_analog_read(int pin);
SIM_IMPORT(sim_digital_read) int sim_digital_read(int pin);
SIM_IMPORT(sim_digital_write) void sim_digital_write(int pin, int value);
}

typedef bool boolean;
typedef uint8_t byte;
typedef unsigned int word;

#define PROGMEM
#define PGM_P const char *
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#define pgm_read_float(addr) (*(const float *)(addr))
#define pgm_read_ptr(addr) (*(void *const *)(addr))
#define memcpy_P memcpy
#define strlen_P strlen
#define strcpy_P strcpy

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LED_BUILTIN 13
#define A0 0
#define A1 1
#define A2 2
#define A3 3

#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define sq(x) ((x) * (x))
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bit(b) (1UL << (b))
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void yield();
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);
long random(long max);
long random(long min, long max);
void randomSeed(unsigned long seed);
long map(long x, long in_min, long in_max, long out_min, long out_max);

class String;

class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *buffer, size_t size);
  size_t write(const char *str) { return str ? write((const uint8_t *)str, strlen(str)) : 0; }
  size_t write(const char *buffer, size_t size) { return write((const uint8_t *)buffer, size); }

  size_t print(const __FlashStringHelper *s) { return print((const char *)s); }
  size_t print(const String &s);
  size_t print(const char s[]) { return write(s); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(unsigned char n, int base = DEC) { return print((unsigned long)n, base); }
  size_t print(int n, int base = DEC) { return print((long)n, base); }
  size_t print(unsigned int n, int base = DEC) { return print((unsigned long)n, base); }
  size_t print(long n, int base = DEC);
  size_t print(unsigned long n, int base = DEC);
  size_t print(long long n, int base = DEC) { return print((long)n, base); }
  size_t print(unsigned long long n, int base = DEC) { return print((unsigned long)n, base); }
  size_t print(double n, int digits = 2);
  size_t print(bool b) { return print((int)b); }

  size_t println() { return write("\r\n"); }
  template <typename T> size_t println(T v) { size_t n = print(v); return n + println(); }
  template <typename T> size_t println(T v, int f) { size_t n = print(v, f); return n + println(); }

  size_t printf(const char *fmt, ...);
  virtual void flush() {}
};

class Stream : public Print {
 public:
  virtual int available() { return 0; }
  virtual int read() { return -1; }
  virtual int peek() { return -1; }
};

class HardwareSerial : public Stream {
 public:
  void begin(unsigned long baud);
  void begin(unsigned long baud, int) { begin(baud); }
  void end() {}
  unsigned long baud() const { return baud_; }
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  using Print::write;
  operator bool() { return true; }
 private:
  unsigned long baud_ = 0;
};

extern HardwareSerial Serial;

// A small Arduino-style String.
class String {
 public:
  String(const char *s = "") { set(s ? s : ""); }
  String(const String &o) { set(o.buf_); }
  String(const __FlashStringHelper *s) { set((const char *)s); }
  String(char c) { char b[2] = {c, 0}; set(b); }
  String(int v, int base = DEC) { fromLong(v, base); }
  String(unsigned int v, int base = DEC) { fromULong(v, base); }
  String(long v, int base = DEC) { fromLong(v, base); }
  String(unsigned long v, int base = DEC) { fromULong(v, base); }
  String(float v, int digits = 2) { fromDouble(v, digits); }
  String(double v, int digits = 2) { fromDouble(v, digits); }
  ~String() { free(buf_); }
  String &operator=(const String &o) { if (this != &o) set(o.buf_); return *this; }
  String &operator=(const char *s) { set(s ? s : ""); return *this; }
  String &operator+=(const String &o) { append(o.buf_); return *this; }
  String &operator+=(const char *s) { append(s); return *this; }
  String &operator+=(char c) { char b[2] = {c, 0}; append(b); return *this; }
  String &operator+=(int v) { return *this += String(v); }
  String &operator+=(long v) { return *this += String(v); }
  String &operator+=(unsigned long v) { return *this += String(v); }
  String &operator+=(double v) { return *this += String(v); }
  template <typename T> String &concat(T v) { return *this += v; }
  friend String operator+(const String &a, const String &b) { String r(a); r += b; return r; }
  friend String operator+(const String &a, const char *b) { String r(a); r += b; return r; }
  friend String operator+(const char *a, const String &b) { String r(a); r += b; return r; }
  bool operator==(const String &o) const { return strcmp(buf_, o.buf_) == 0; }
  bool operator==(const char *s) const { return strcmp(buf_, s) == 0; }
  bool operator!=(const String &o) const { return !(*this == o); }
  const char *c_str() const { return buf_; }
  unsigned int length() const { return len_; }
  char charAt(unsigned int i) const { return i < len_ ? buf_[i] : 0; }
  char operator[](unsigned int i) const { return charAt(i); }
  int indexOf(char c) const { const char *p = strchr(buf_, c); return p ? (int)(p - buf_) : -1; }
  String substring(unsigned int from, unsigned int to = 0xffffffff) const {
    if (to > len_) to = len_;
    if (from > to) from = to;
    String r; free(r.buf_); r.len_ = to - from; r.buf_ = (char *)malloc(r.len_ + 1);
    memcpy(r.buf_, buf_ + from, r.len_); r.buf_[r.len_] = 0; return r;
  }
  long toInt() const { return atol(buf_); }
  float toFloat() const { return (float)atof(buf_); }
 private:
  void set(const char *s) { size_t n = strlen(s); char *b = (char *)malloc(n + 1); memcpy(b, s, n + 1); free(buf_); buf_ = b; len_ = n; }
  void append(const char *s) { size_t n = strlen(s); buf_ = (char *)realloc(buf_, len_ + n + 1); memcpy(buf_ + len_, s, n + 1); len_ += n; }
  void fromLong(long v, int base) { char b[40]; if (base == 10) snprintf(b, sizeof b, "%ld", v); else fromULong((unsigned long)v, base); if (base == 10) set(b); }
  void fromULong(unsigned long v, int base) { char b[40]; int i = 39; b[i] = 0; if (base < 2) base = 10; do { int d = v % base; b[--i] = d < 10 ? '0' + d : 'A' + d - 10; v /= base; } while (v); set(b + i); }
  void fromDouble(double v, int digits) { char b[64]; snprintf(b, sizeof b, "%.*f", digits, v); set(b); }
  char *buf_ = nullptr;
  size_t len_ = 0;
};

inline size_t Print::print(const String &s) { return write(s.c_str()); }

void setup();
void loop();
