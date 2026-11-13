#pragma once
#include <Arduino.h>

// Stand-in for Adafruit_GFX: same drawing calls and the same algorithms,
// written for the simulator. Text uses a built-in 5x7 font (close to the
// Adafruit "classic" font, not byte-for-byte identical). Custom GFXfonts work.

typedef struct {
  uint16_t bitmapOffset;
  uint8_t width, height, xAdvance;
  int8_t xOffset, yOffset;
} GFXglyph;

typedef struct {
  uint8_t *bitmap;
  GFXglyph *glyph;
  uint16_t first, last;
  uint8_t yAdvance;
} GFXfont;

class Adafruit_GFX : public Print {
 public:
  Adafruit_GFX(int16_t w, int16_t h);
  virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;

  virtual void startWrite() {}
  virtual void writePixel(int16_t x, int16_t y, uint16_t color) { drawPixel(x, y, color); }
  virtual void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { fillRect(x, y, w, h, color); }
  virtual void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) { drawFastVLine(x, y, h, color); }
  virtual void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { drawFastHLine(x, y, w, color); }
  virtual void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  virtual void endWrite() {}

  virtual void setRotation(uint8_t r);
  virtual void invertDisplay(bool) {}
  virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  virtual void fillScreen(uint16_t color) { fillRect(0, 0, _width, _height, color); }
  virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

  void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
  void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color);
  void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
  void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, uint16_t color);
  void drawEllipse(int16_t x0, int16_t y0, int16_t rw, int16_t rh, uint16_t color);
  void fillEllipse(int16_t x0, int16_t y0, int16_t rw, int16_t rh, uint16_t color);
  void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
  void drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color);
  void fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color);
  void drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color);
  void drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color, uint16_t bg);
  void drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) { drawBitmap(x, y, (const uint8_t *)bitmap, w, h, color); }
  void drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg) { drawBitmap(x, y, (const uint8_t *)bitmap, w, h, color, bg); }
  void drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color);
  void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size) { drawChar(x, y, c, color, bg, size, size); }
  void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size_x, uint8_t size_y);
  void getTextBounds(const char *string, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h);
  void getTextBounds(const __FlashStringHelper *s, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) { getTextBounds((const char *)s, x, y, x1, y1, w, h); }
  void getTextBounds(const String &str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) { getTextBounds(str.c_str(), x, y, x1, y1, w, h); }
  void setTextSize(uint8_t s) { setTextSize(s, s); }
  void setTextSize(uint8_t sx, uint8_t sy) { textsize_x = sx > 0 ? sx : 1; textsize_y = sy > 0 ? sy : 1; }
  void setFont(const GFXfont *f = NULL);
  void setCursor(int16_t x, int16_t y) { cursor_x = x; cursor_y = y; }
  void setTextColor(uint16_t c) { textcolor = textbgcolor = c; }
  void setTextColor(uint16_t c, uint16_t bg) { textcolor = c; textbgcolor = bg; }
  void setTextWrap(bool w) { wrap = w; }
  void cp437(bool x = true) { _cp437 = x; }

  using Print::write;
  virtual size_t write(uint8_t) override;

  int16_t width() const { return _width; }
  int16_t height() const { return _height; }
  uint8_t getRotation() const { return rotation; }
  int16_t getCursorX() const { return cursor_x; }
  int16_t getCursorY() const { return cursor_y; }

 protected:
  void charBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy);
  int16_t WIDTH, HEIGHT;
  int16_t _width, _height;
  int16_t cursor_x, cursor_y;
  uint16_t textcolor, textbgcolor;
  uint8_t textsize_x, textsize_y;
  uint8_t rotation;
  bool wrap;
  bool _cp437;
  GFXfont *gfxFont;
};
