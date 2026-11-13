#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <SPI.h>

// Stand-in for Adafruit_SSD1306. Pixels go into the same 1-bit page buffer
// layout as the real library; display() hands the buffer to the simulator,
// which also charges the time an I2C transfer of the frame takes.
#define BLACK 0
#define WHITE 1
#define INVERSE 2
#define SSD1306_BLACK 0
#define SSD1306_WHITE 1
#define SSD1306_INVERSE 2
#define SSD1306_EXTERNALVCC 0x01
#define SSD1306_SWITCHCAPVCC 0x02
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_SETCONTRAST 0x81

class Adafruit_SSD1306 : public Adafruit_GFX {
 public:
  Adafruit_SSD1306(uint8_t w, uint8_t h, TwoWire *twi = &Wire, int8_t rst_pin = -1, uint32_t clkDuring = 400000UL, uint32_t clkAfter = 100000UL);
  Adafruit_SSD1306(uint8_t w, uint8_t h, int8_t mosi, int8_t sclk, int8_t dc, int8_t rst, int8_t cs);
  Adafruit_SSD1306(uint8_t w, uint8_t h, SPIClass *spi, int8_t dc, int8_t rst, int8_t cs, uint32_t bitrate = 8000000UL);
  Adafruit_SSD1306(int8_t rst_pin = -1);  // old style, 128x64
  ~Adafruit_SSD1306() {}

  bool begin(uint8_t switchvcc = SSD1306_SWITCHCAPVCC, uint8_t i2caddr = 0, bool reset = true, bool periphBegin = true);
  void display();
  void clearDisplay();
  void invertDisplay(bool i) override;
  void dim(bool dim);
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void startscrollright(uint8_t, uint8_t) {}
  void startscrollleft(uint8_t, uint8_t) {}
  void startscrolldiagright(uint8_t, uint8_t) {}
  void startscrolldiagleft(uint8_t, uint8_t) {}
  void stopscroll() {}
  void ssd1306_command(uint8_t c);
  bool getPixel(int16_t x, int16_t y);
  uint8_t *getBuffer() { return buffer; }

 private:
  uint8_t buffer[128 * 64 / 8];
  uint8_t i2caddr_ = 0x3C;
  uint32_t clkDuring_ = 400000UL;
  bool begun_ = false;
  bool inverted_ = false;
  bool on_ = true;
  uint8_t contrast_ = 0xCF;
  uint8_t pendingCmd_ = 0;
};
