#pragma once
#include <Arduino.h>
#include <Wire.h>

// Stand-in for the Seeed Grove ADXL345 library (ADXL345.h). It talks to the
// simulated chip's registers, so things like forgetting powerOn() or a
// +/-2 g range clipping hard hits behave the way the real part does.
#define ADXL345_DEVICE 0x53
#define ADXL345_INT1_PIN 0x00
#define ADXL345_INT2_PIN 0x01
#define ADXL345_INT_SINGLE_TAP_BIT 0x06
#define ADXL345_INT_DOUBLE_TAP_BIT 0x05
#define ADXL345_INT_ACTIVITY_BIT 0x04
#define ADXL345_INT_INACTIVITY_BIT 0x03
#define ADXL345_INT_FREE_FALL_BIT 0x02
#define ADXL345_DATA_READY 0x07
#define ADXL345_SINGLE_TAP 0x06
#define ADXL345_DOUBLE_TAP 0x05
#define ADXL345_ACTIVITY 0x04
#define ADXL345_INACTIVITY 0x03
#define ADXL345_FREE_FALL 0x02

class ADXL345 {
 public:
  bool status = true;
  uint8_t error_code = 0;
  double gains[3] = {0.00376390, 0.00376009, 0.00349265};

  void powerOn() {
    sim_adxl_write_reg(0x2D, 0);
    sim_adxl_write_reg(0x2D, 16);
    sim_adxl_write_reg(0x2D, 8);
  }
  void readXYZ(int *x, int *y, int *z) {
    uint8_t b[6];
    for (int i = 0; i < 6; i++) b[i] = sim_adxl_read_reg(0x32 + i);
    *x = (int16_t)((b[1] << 8) | b[0]);
    *y = (int16_t)((b[3] << 8) | b[2]);
    *z = (int16_t)((b[5] << 8) | b[4]);
  }
  void readAccel(int *xyz) { readXYZ(xyz, xyz + 1, xyz + 2); }
  void readAccel(int *x, int *y, int *z) { readXYZ(x, y, z); }
  void getAcceleration(double *xyz) {
    int i[3];
    readXYZ(i, i + 1, i + 2);
    for (int k = 0; k < 3; k++) xyz[k] = i[k] * gains[k];
  }
  void setRangeSetting(int val) {
    uint8_t s = val == 16 ? 3 : val == 8 ? 2 : val == 4 ? 1 : 0;
    sim_adxl_write_reg(0x31, (sim_adxl_read_reg(0x31) & 0xEC) | s);
  }
  void getRangeSetting(uint8_t *r) { *r = 2 << (sim_adxl_read_reg(0x31) & 3); }
  void setFullResBit(bool on) { setBit(0x31, 3, on); }
  bool getFullResBit() { return sim_adxl_read_reg(0x31) & 8; }
  void setRate(double) {}
  double getRate() { return 100; }
  void setAxisGains(double *g) { for (int k = 0; k < 3; k++) gains[k] = g[k]; }
  void getAxisGains(double *g) { for (int k = 0; k < 3; k++) g[k] = gains[k]; }
  void setAxisOffset(int, int, int) {}
  void getAxisOffset(int *x, int *y, int *z) { *x = *y = *z = 0; }
  void setTapThreshold(int) {}
  void setTapDuration(int) {}
  void setDoubleTapLatency(int) {}
  void setDoubleTapWindow(int) {}
  void setActivityThreshold(int) {}
  void setInactivityThreshold(int) {}
  void setTimeInactivity(int) {}
  void setFreeFallThreshold(int) {}
  void setFreeFallDuration(int) {}
  void setActivityX(bool) {}
  void setActivityY(bool) {}
  void setActivityZ(bool) {}
  void setActivityXYZ(bool, bool, bool) {}
  void setInactivityX(bool) {}
  void setInactivityY(bool) {}
  void setInactivityZ(bool) {}
  void setInactivityXYZ(bool, bool, bool) {}
  void setActivityAc(bool) {}
  void setInactivityAc(bool) {}
  void setTapDetectionOnX(bool) {}
  void setTapDetectionOnY(bool) {}
  void setTapDetectionOnZ(bool) {}
  void setTapDetectionOnXYZ(bool, bool, bool) {}
  void setSuppressBit(bool) {}
  void setLowPower(bool) {}
  void setInterruptLevelBit(bool) {}
  void setInterruptMapping(uint8_t, bool) {}
  void setInterrupt(uint8_t, bool) {}
  void setImportantInterruptMapping(int, int, int, int, int) {}
  void InactivityINT(bool) {}
  void ActivityINT(bool) {}
  void FreeFallINT(bool) {}
  void doubleTapINT(bool) {}
  void singleTapINT(bool) {}
  uint8_t getInterruptSource() { return 0; }
  bool getInterruptSource(uint8_t) { return false; }
  bool triggered(uint8_t, int) { return false; }
  bool isInterruptEnabled(uint8_t) { return false; }

 private:
  void setBit(uint8_t reg, int bit, bool on) {
    uint8_t v = sim_adxl_read_reg(reg);
    sim_adxl_write_reg(reg, on ? (v | (1 << bit)) : (v & ~(1 << bit)));
  }
};
