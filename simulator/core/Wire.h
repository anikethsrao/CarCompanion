#pragma once
#include <Arduino.h>

// I2C bus. The simulator puts an ADXL345 at 0x53 and an SSD1306 at 0x3C/0x3D,
// so sketches that talk to the accelerometer registers directly also work.
class TwoWire : public Stream {
 public:
  void begin() {}
  void begin(uint8_t) {}
  void end() {}
  void setClock(uint32_t hz) { clock_ = hz; }
  uint32_t getClock() const { return clock_; }
  void beginTransmission(uint8_t address);
  void beginTransmission(int address) { beginTransmission((uint8_t)address); }
  uint8_t endTransmission(bool sendStop = true);
  uint8_t requestFrom(uint8_t address, size_t quantity, bool sendStop = true);
  uint8_t requestFrom(int address, int quantity, int sendStop = 1) { return requestFrom((uint8_t)address, (size_t)quantity, sendStop != 0); }
  size_t write(uint8_t data) override;
  size_t write(const uint8_t *data, size_t quantity) override;
  using Print::write;
  int available() override { return rxLen_ - rxPos_; }
  int read() override { return rxPos_ < rxLen_ ? rxBuf_[rxPos_++] : -1; }
  int peek() override { return rxPos_ < rxLen_ ? rxBuf_[rxPos_] : -1; }

 private:
  uint8_t txAddr_ = 0;
  uint8_t txBuf_[64];
  int txLen_ = 0;
  uint8_t rxBuf_[64];
  int rxLen_ = 0, rxPos_ = 0;
  int regPtr_ = 0;  // last register pointer written to the ADXL345
  uint32_t clock_ = 100000;
};

extern TwoWire Wire;

// Simulated ADXL345 register file (used by Wire and by the ADXL345 library).
uint8_t sim_adxl_read_reg(uint8_t reg);
void sim_adxl_write_reg(uint8_t reg, uint8_t val);
bool sim_i2c_device_present(uint8_t address);
