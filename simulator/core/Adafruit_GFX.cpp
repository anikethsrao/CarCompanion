#include "Adafruit_GFX.h"
#include "font5x7.h"

#define _swap(a, b) { int16_t t = a; a = b; b = t; }

Adafruit_GFX::Adafruit_GFX(int16_t w, int16_t h) : WIDTH(w), HEIGHT(h) {
  _width = WIDTH;
  _height = HEIGHT;
  rotation = 0;
  cursor_y = cursor_x = 0;
  textsize_x = textsize_y = 1;
  textcolor = textbgcolor = 0xFFFF;
  wrap = true;
  _cp437 = false;
  gfxFont = NULL;
}

void Adafruit_GFX::writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) { _swap(x0, y0); _swap(x1, y1); }
  if (x0 > x1) { _swap(x0, x1); _swap(y0, y1); }
  int16_t dx = x1 - x0, dy = abs(y1 - y0);
  int16_t err = dx / 2, ystep = y0 < y1 ? 1 : -1;
  for (; x0 <= x1; x0++) {
    if (steep) writePixel(y0, x0, color); else writePixel(x0, y0, color);
    err -= dy;
    if (err < 0) { y0 += ystep; err += dx; }
  }
}

void Adafruit_GFX::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) { writeLine(x, y, x, y + h - 1, color); }
void Adafruit_GFX::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { writeLine(x, y, x + w - 1, y, color); }
void Adafruit_GFX::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  for (int16_t i = x; i < x + w; i++) writeFastVLine(i, y, h, color);
}

void Adafruit_GFX::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  if (x0 == x1) { if (y0 > y1) _swap(y0, y1); drawFastVLine(x0, y0, y1 - y0 + 1, color); }
  else if (y0 == y1) { if (x0 > x1) _swap(x0, x1); drawFastHLine(x0, y0, x1 - x0 + 1, color); }
  else writeLine(x0, y0, x1, y1, color);
}

void Adafruit_GFX::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
  writePixel(x0, y0 + r, color); writePixel(x0, y0 - r, color);
  writePixel(x0 + r, y0, color); writePixel(x0 - r, y0, color);
  while (x < y) {
    if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
    x++; ddF_x += 2; f += ddF_x;
    writePixel(x0 + x, y0 + y, color); writePixel(x0 - x, y0 + y, color);
    writePixel(x0 + x, y0 - y, color); writePixel(x0 - x, y0 - y, color);
    writePixel(x0 + y, y0 + x, color); writePixel(x0 - y, y0 + x, color);
    writePixel(x0 + y, y0 - x, color); writePixel(x0 - y, y0 - x, color);
  }
}

void Adafruit_GFX::drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color) {
  int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
  while (x < y) {
    if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
    x++; ddF_x += 2; f += ddF_x;
    if (cornername & 0x4) { writePixel(x0 + x, y0 + y, color); writePixel(x0 + y, y0 + x, color); }
    if (cornername & 0x2) { writePixel(x0 + x, y0 - y, color); writePixel(x0 + y, y0 - x, color); }
    if (cornername & 0x8) { writePixel(x0 - y, y0 + x, color); writePixel(x0 - x, y0 + y, color); }
    if (cornername & 0x1) { writePixel(x0 - y, y0 - x, color); writePixel(x0 - x, y0 - y, color); }
  }
}

void Adafruit_GFX::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  writeFastVLine(x0, y0 - r, 2 * r + 1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

void Adafruit_GFX::fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, uint16_t color) {
  int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r, px = x, py = y;
  delta++;
  while (x < y) {
    if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
    x++; ddF_x += 2; f += ddF_x;
    if (x < (y + 1)) {
      if (corners & 1) writeFastVLine(x0 + x, y0 - y, 2 * y + delta, color);
      if (corners & 2) writeFastVLine(x0 - x, y0 - y, 2 * y + delta, color);
    }
    if (y != py) {
      if (corners & 1) writeFastVLine(x0 + py, y0 - px, 2 * px + delta, color);
      if (corners & 2) writeFastVLine(x0 - py, y0 - px, 2 * px + delta, color);
      py = y;
    }
    px = x;
  }
}

void Adafruit_GFX::drawEllipse(int16_t x0, int16_t y0, int16_t rw, int16_t rh, uint16_t color) {
  for (int a = 0; a < 360; a++) {
    double t = a * DEG_TO_RAD;
    writePixel(x0 + (int16_t)lround(rw * cos(t)), y0 + (int16_t)lround(rh * sin(t)), color);
  }
}

void Adafruit_GFX::fillEllipse(int16_t x0, int16_t y0, int16_t rw, int16_t rh, uint16_t color) {
  for (int16_t dy = -rh; dy <= rh; dy++) {
    double k = rh ? 1.0 - (double)(dy * dy) / (rh * rh) : 1.0;
    int16_t dx = (int16_t)lround(rw * sqrt(k > 0 ? k : 0));
    writeFastHLine(x0 - dx, y0 + dy, 2 * dx + 1, color);
  }
}

void Adafruit_GFX::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  writeFastHLine(x, y, w, color);
  writeFastHLine(x, y + h - 1, w, color);
  writeFastVLine(x, y, h, color);
  writeFastVLine(x + w - 1, y, h, color);
}

void Adafruit_GFX::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  int16_t max_radius = ((w < h) ? w : h) / 2;
  if (r > max_radius) r = max_radius;
  writeFastHLine(x + r, y, w - 2 * r, color);
  writeFastHLine(x + r, y + h - 1, w - 2 * r, color);
  writeFastVLine(x, y + r, h - 2 * r, color);
  writeFastVLine(x + w - 1, y + r, h - 2 * r, color);
  drawCircleHelper(x + r, y + r, r, 1, color);
  drawCircleHelper(x + w - r - 1, y + r, r, 2, color);
  drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
  drawCircleHelper(x + r, y + h - r - 1, r, 8, color);
}

void Adafruit_GFX::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  int16_t max_radius = ((w < h) ? w : h) / 2;
  if (r > max_radius) r = max_radius;
  writeFillRect(x + r, y, w - 2 * r, h, color);
  fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
  fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
}

void Adafruit_GFX::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

void Adafruit_GFX::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  int16_t a, b, y, last;
  if (y0 > y1) { _swap(y0, y1); _swap(x0, x1); }
  if (y1 > y2) { _swap(y2, y1); _swap(x2, x1); }
  if (y0 > y1) { _swap(y0, y1); _swap(x0, x1); }
  if (y0 == y2) {
    a = b = x0;
    if (x1 < a) a = x1; else if (x1 > b) b = x1;
    if (x2 < a) a = x2; else if (x2 > b) b = x2;
    writeFastHLine(a, y0, b - a + 1, color);
    return;
  }
  int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0, dx12 = x2 - x1, dy12 = y2 - y1;
  int32_t sa = 0, sb = 0;
  last = (y1 == y2) ? y1 : y1 - 1;
  for (y = y0; y <= last; y++) {
    a = x0 + sa / dy01; b = x0 + sb / dy02;
    sa += dx01; sb += dx02;
    if (a > b) _swap(a, b);
    writeFastHLine(a, y, b - a + 1, color);
  }
  sa = (int32_t)dx12 * (y - y1);
  sb = (int32_t)dx02 * (y - y0);
  for (; y <= y2; y++) {
    a = x1 + sa / dy12; b = x0 + sb / dy02;
    sa += dx12; sb += dx02;
    if (a > b) _swap(a, b);
    writeFastHLine(a, y, b - a + 1, color);
  }
}

extern "C" SIM_IMPORT(sim_draw_bitmap) void sim_draw_bitmap(const uint8_t *bitmap, int x, int y, int w, int h);

void Adafruit_GFX::drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color) {
  sim_draw_bitmap(bitmap, x, y, w, h);
  int16_t byteWidth = (w + 7) / 8;
  uint8_t b = 0;
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7) b <<= 1; else b = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      if (b & 0x80) writePixel(x + i, y, color);
    }
  }
}

void Adafruit_GFX::drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color, uint16_t bg) {
  sim_draw_bitmap(bitmap, x, y, w, h);
  int16_t byteWidth = (w + 7) / 8;
  uint8_t b = 0;
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7) b <<= 1; else b = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      writePixel(x + i, y, (b & 0x80) ? color : bg);
    }
  }
}

void Adafruit_GFX::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color) {
  sim_draw_bitmap(bitmap, x, y, w, h);
  int16_t byteWidth = (w + 7) / 8;
  uint8_t b = 0;
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7) b >>= 1; else b = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      if (b & 0x01) writePixel(x + i, y, color);
    }
  }
}

void Adafruit_GFX::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size_x, uint8_t size_y) {
  if (!gfxFont) {
    if ((x >= _width) || (y >= _height) || ((x + 6 * size_x - 1) < 0) || ((y + 8 * size_y - 1) < 0)) return;
    const unsigned char *glyph = (c >= 0x20 && c <= 0x7E) ? sim_font5x7[c - 0x20] : NULL;
    for (int8_t i = 0; i < 5; i++) {
      uint8_t line = glyph ? glyph[i] : (i == 0 || i == 4 ? 0x7F : 0x41);  // box for unknown chars
      for (int8_t j = 0; j < 8; j++, line >>= 1) {
        if (line & 1) {
          if (size_x == 1 && size_y == 1) writePixel(x + i, y + j, color);
          else writeFillRect(x + i * size_x, y + j * size_y, size_x, size_y, color);
        } else if (bg != color) {
          if (size_x == 1 && size_y == 1) writePixel(x + i, y + j, bg);
          else writeFillRect(x + i * size_x, y + j * size_y, size_x, size_y, bg);
        }
      }
    }
    if (bg != color) {
      if (size_x == 1 && size_y == 1) writeFastVLine(x + 5, y, 8, bg);
      else writeFillRect(x + 5 * size_x, y, size_x, 8 * size_y, bg);
    }
  } else {
    c -= (uint8_t)gfxFont->first;
    GFXglyph *glyph = &gfxFont->glyph[c];
    uint8_t *bitmap = gfxFont->bitmap;
    uint16_t bo = glyph->bitmapOffset;
    uint8_t w = glyph->width, h = glyph->height;
    int8_t xo = glyph->xOffset, yo = glyph->yOffset;
    uint8_t xx, yy, bits = 0, bit = 0;
    int16_t xo16 = xo, yo16 = yo;
    for (yy = 0; yy < h; yy++) {
      for (xx = 0; xx < w; xx++) {
        if (!(bit++ & 7)) bits = bitmap[bo++];
        if (bits & 0x80) {
          if (size_x == 1 && size_y == 1) writePixel(x + xo + xx, y + yo + yy, color);
          else writeFillRect(x + (xo16 + xx) * size_x, y + (yo16 + yy) * size_y, size_x, size_y, color);
        }
        bits <<= 1;
      }
    }
  }
}

size_t Adafruit_GFX::write(uint8_t c) {
  if (!gfxFont) {
    if (c == '\n') { cursor_x = 0; cursor_y += textsize_y * 8; }
    else if (c != '\r') {
      if (wrap && ((cursor_x + textsize_x * 6) > _width)) { cursor_x = 0; cursor_y += textsize_y * 8; }
      drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize_x, textsize_y);
      cursor_x += textsize_x * 6;
    }
  } else {
    if (c == '\n') { cursor_x = 0; cursor_y += (int16_t)textsize_y * gfxFont->yAdvance; }
    else if (c != '\r') {
      uint8_t first = gfxFont->first;
      if ((c >= first) && (c <= gfxFont->last)) {
        GFXglyph *glyph = &gfxFont->glyph[c - first];
        uint8_t w = glyph->width, h = glyph->height;
        if ((w > 0) && (h > 0)) {
          int16_t xo = glyph->xOffset;
          if (wrap && ((cursor_x + textsize_x * (xo + w)) > _width)) {
            cursor_x = 0;
            cursor_y += (int16_t)textsize_y * gfxFont->yAdvance;
          }
          drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize_x, textsize_y);
        }
        cursor_x += glyph->xAdvance * (int16_t)textsize_x;
      }
    }
  }
  return 1;
}

void Adafruit_GFX::setRotation(uint8_t x) {
  rotation = (x & 3);
  switch (rotation) {
    case 0: case 2: _width = WIDTH; _height = HEIGHT; break;
    case 1: case 3: _width = HEIGHT; _height = WIDTH; break;
  }
}

void Adafruit_GFX::setFont(const GFXfont *f) {
  if (f) { if (!gfxFont) cursor_y += 6; }
  else if (gfxFont) cursor_y -= 6;
  gfxFont = (GFXfont *)f;
}

void Adafruit_GFX::charBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy) {
  if (gfxFont) {
    if (c == '\n') { *x = 0; *y += textsize_y * gfxFont->yAdvance; }
    else if (c != '\r') {
      uint8_t first = gfxFont->first, last = gfxFont->last;
      if ((c >= first) && (c <= last)) {
        GFXglyph *glyph = &gfxFont->glyph[c - first];
        uint8_t gw = glyph->width, gh = glyph->height, xa = glyph->xAdvance;
        int8_t xo = glyph->xOffset, yo = glyph->yOffset;
        if (wrap && ((*x + (((int16_t)xo + gw) * textsize_x)) > _width)) { *x = 0; *y += textsize_y * gfxFont->yAdvance; }
        int16_t tsx = textsize_x, tsy = textsize_y, x1 = *x + xo * tsx, y1 = *y + yo * tsy, x2 = x1 + gw * tsx - 1, y2 = y1 + gh * tsy - 1;
        if (x1 < *minx) *minx = x1;
        if (y1 < *miny) *miny = y1;
        if (x2 > *maxx) *maxx = x2;
        if (y2 > *maxy) *maxy = y2;
        *x += xa * tsx;
      }
    }
  } else {
    if (c == '\n') { *x = 0; *y += textsize_y * 8; }
    else if (c != '\r') {
      if (wrap && ((*x + textsize_x * 6) > _width)) { *x = 0; *y += textsize_y * 8; }
      int x2 = *x + textsize_x * 6 - 1, y2 = *y + textsize_y * 8 - 1;
      if (x2 > *maxx) *maxx = x2;
      if (y2 > *maxy) *maxy = y2;
      if (*x < *minx) *minx = *x;
      if (*y < *miny) *miny = *y;
      *x += textsize_x * 6;
    }
  }
}

void Adafruit_GFX::getTextBounds(const char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  uint8_t c;
  int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1;
  *x1 = x; *y1 = y; *w = *h = 0;
  while ((c = *str++)) charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy);
  if (maxx >= minx) { *x1 = minx; *w = maxx - minx + 1; }
  if (maxy >= miny) { *y1 = miny; *h = maxy - miny + 1; }
}
