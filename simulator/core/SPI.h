#pragma once
#include <Arduino.h>
// SPI is not simulated; this exists so sketches that include it still build.
class SPISettings { public: SPISettings() {} SPISettings(uint32_t, uint8_t, uint8_t) {} };
class SPIClass {
 public:
  void begin() {}
  void end() {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t) { return 0; }
};
extern SPIClass SPI;
#define MSBFIRST 1
#define SPI_MODE0 0
