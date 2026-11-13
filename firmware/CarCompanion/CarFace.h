// CarFace.h - procedural, spring-animated face for the Car Companion.
//
// Instead of flipping between fixed bitmaps, the face is described by ~18
// numbers (eye size, lid, smile curve, brow tilt...). Every mood is a target
// set of those numbers and each one eases toward its target on a damped
// spring, so any expression blends smoothly into any other. On top of that
// the face blinks, glances around, breathes, and leans into turns.
//
// Header-only. Works with any display class that has
//   fillScreen(c), drawFastHLine(x,y,w,c), drawFastVLine(x,y,h,c), display()
// e.g. Adafruit_SSD1306. Uses no bitmaps, ~200 bytes of RAM.
//
//   Adafruit_SSD1306 display(128, 64, &Wire, -1);
//   carface::CarFace<Adafruit_SSD1306> face(display);
//   void setup() { display.begin(SSD1306_SWITCHCAPVCC, 0x3C); face.begin(millis()); }
//   void loop()  { face.drive(forwardG, lateralG, millis()); face.update(millis()); }
//
// drive() is optional: call setMood()/flash()/setLean()/wake() yourself
// if you'd rather keep your own detection logic.
#pragma once
#include <math.h>
#include <stdint.h>

namespace carface {

enum Mood : uint8_t {
  NEUTRAL, CONTENT, JOY, SHOCKED, SLEEPY, ASLEEP, SPEEDING, BRAKING, MOOD_COUNT
};

enum Param : uint8_t {
  LOOK_X, LOOK_Y, EYE_SPREAD, EYE_Y, EYE_R, EYE_SKEW, LID, HAPPY, SQUEEZE,
  BROW, BROW_TILT, MOUTH_W, MOUTH_CURVE, MOUTH_OPEN, MOUTH_GRIN, MOUTH_THICK,
  MOUTH_Y, SPEED_FX, PARAM_COUNT
};

// Target face for each mood (pixels on a 128x64 screen unless noted).
static const float PRESETS[MOOD_COUNT][PARAM_COUNT] = {
  // lookX lookY spread eyeY eyeR skew  lid  happy sqz  brow tilt  mW  curve open grin thick mY  fx
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    24,  0,   0,   0,   5,   47,  0 }, // NEUTRAL
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    32,  3,   0,   0,   5,   47,  0 }, // CONTENT
  {  0,    1,    40,    24,  12,  0,    0,   1,    0,   0,   0,    24,  0,   0,   5,   4,   45,  0 }, // JOY
  {  0,    0,    42,    24,  15,  0,    0,   0,    0,   0.6, 0,    15,  0,   1,   0,   4,   49,  0 }, // SHOCKED
  {  0,    2,    41,    24,  11,  0,    0.62,0,    0,   0,   0,    20,  0,   0,   0,   4,   48,  0 }, // SLEEPY
  {  0,    3,    41,    26,  11,  0,    1,   0,    0,   0,   0,    28,  0,   0,   0,   4,   48,  0 }, // ASLEEP
  {  0,    0,    33,    24,   8,  0,    0,   0,    0,   0.8, 0.6,  10,  0,   1,   0,   3,   46,  1 }, // SPEEDING
  {  0,    3,    41,    23,  11,  0,    0,   0,    1,   1,  -0.7,  22, -4,   0,   0,   4,   48,  0 }, // BRAKING
};

// Spring stiffness and damping ratio per mood: low damping = bouncy pop.
static const float SPRING_K[MOOD_COUNT]    = { 170, 140, 220, 600,  20,  12, 300, 450 };
static const float SPRING_ZETA[MOOD_COUNT] = { 0.8, 0.8, 0.6, 0.42, 1.0, 1.0, 0.55, 0.5 };

static const char* const MOOD_NAMES[MOOD_COUNT] = {
  "neutral", "content", "joy", "shocked", "sleepy", "asleep", "speeding", "braking"
};

template <class D>
class CarFace {
 public:
  explicit CarFace(D& d) : d_(d) {}

  void begin(uint32_t nowMs, uint32_t seed = 0x2545F491u) {
    rng_ = seed ? seed : 1;
    for (int i = 0; i < PARAM_COUNT; i++) { cur_[i] = PRESETS[ASLEEP][i]; vel_[i] = 0; }
    base_ = ASLEEP; flash_ = MOOD_COUNT; springMood_ = MOOD_COUNT;
    t_ = 0; lastMs_ = nowMs; lastFrameMs_ = nowMs - frameMs_;
    nextBlink_ = 1.5f; nextGaze_ = 1.0f; blinkStart_ = -10;
    queueLen_ = 0; lean_ = 0; gx_ = gy_ = gvx_ = gvy_ = gtx_ = gty_ = 0;
    driveLastMs_ = nowMs; lastActiveMs_ = nowMs; calmSinceMs_ = nowMs;
    holdAcc_ = holdBrk_ = 0; prevLon_ = 0;
    wake();  // boot-up: open eyes, blink, smile
  }

  // Base mood the face settles into.
  void setMood(Mood m) { base_ = m; }
  // Show a mood for a moment, then return to the base mood.
  void flash(Mood m, uint16_t ms) { flash_ = m; flashUntil_ = t_ + ms / 1000.0f; }
  // -1..1: slide the face left/right, as if pushed by a turn.
  void setLean(float l) { lean_ = clampf(l, -1, 1); }
  // Light background with dark features (original look) or the reverse.
  void setLightBackground(bool on) { light_ = on; }
  void setFrameInterval(uint16_t ms) { frameMs_ = ms; }
  // g needed to flash SHOCKED when speeding up or braking starts.
  void setShockThreshold(float g) { shockG_ = g; }
  void blink() { startBlink(1.0f); }
  Mood mood() const { return flash_ < MOOD_COUNT ? flash_ : base_; }
  Mood baseMood() const { return base_; }
  const float* params() const { return cur_; }

  // Waking up: eyes crack open, a double blink, a happy squint, then settle.
  void wake() {
    queueLen_ = 0; flash_ = MOOD_COUNT;
    base_ = SLEEPY;
    push(0.8f, STEP_MOOD, NEUTRAL);
    push(1.1f, STEP_BLINK, 0);
    push(1.4f, STEP_BLINK, 0);
    push(1.9f, STEP_FLASH, JOY, 1500);
    push(1.9f, STEP_MOOD, CONTENT);
  }

  // Optional automatic behaviour from gravity-corrected acceleration in g.
  // forwardG > 0 speeding up, < 0 braking. lateralG > 0 slides face right.
  void drive(float forwardG, float lateralG, uint32_t nowMs) {
    float dt = (nowMs - driveLastMs_) / 1000.0f;
    driveLastMs_ = nowMs;
    if (dt > 0.2f) dt = 0.2f;

    float l = 0;
    if (lateralG > 0.1f) l = (lateralG - 0.1f) / 0.5f;
    else if (lateralG < -0.1f) l = (lateralG + 0.1f) / 0.5f;
    float act = fmaxf(fabsf(forwardG), fabsf(lateralG));
    if (act > 0.06f) lastActiveMs_ = nowMs;
    if (act > 0.25f) calmSinceMs_ = nowMs;

    if (base_ == ASLEEP || base_ == SLEEPY) {
      setLean(0);
      if (queueLen_ == 0 && act > 0.12f) { wake(); calmSinceMs_ = nowMs; }
      else if (queueLen_ == 0 && base_ == SLEEPY && nowMs - lastActiveMs_ > DROWSY_MS + SLEEP_MS) base_ = ASLEEP;
      prevLon_ = forwardG;
      return;
    }
    setLean(l);

    holdAcc_ = forwardG > 0.45f ? holdAcc_ + dt : 0;
    holdBrk_ = forwardG < -0.45f ? holdBrk_ + dt : 0;
    if (forwardG > shockG_ && prevLon_ <= shockG_) flash(SHOCKED, 700);
    if (forwardG < -shockG_ && prevLon_ >= -shockG_) flash(SHOCKED, 600);
    prevLon_ = forwardG;

    if (holdAcc_ > 0.35f) { base_ = SPEEDING; return; }
    if (holdBrk_ > 0.2f) { base_ = BRAKING; flash_ = MOOD_COUNT; return; }
    if (base_ == SPEEDING || base_ == BRAKING) { base_ = NEUTRAL; calmSinceMs_ = nowMs; }

    uint32_t calm = nowMs - calmSinceMs_;
    if (base_ == NEUTRAL && calm > 6000) base_ = CONTENT;
    if (base_ == CONTENT && calm > 25000) { flash(JOY, 1600); calmSinceMs_ = nowMs; }  // smooth driving!
    if (nowMs - lastActiveMs_ > DROWSY_MS) base_ = SLEEPY;
  }

  // Call every loop. Draws and pushes a frame when one is due.
  bool update(uint32_t nowMs) {
    if ((uint32_t)(nowMs - lastFrameMs_) < frameMs_) return false;
    float dt = (nowMs - lastMs_) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    lastMs_ = nowMs; lastFrameMs_ = nowMs;
    step(dt);
    render();
    d_.display();
    return true;
  }

  // Advance the animation by dt seconds without drawing (used for testing).
  void step(float dt) {
    t_ += dt;
    runQueue();
    if (flash_ < MOOD_COUNT && t_ >= flashUntil_) flash_ = MOOD_COUNT;
    Mood m = mood();
    if (m != springMood_) {
      springMood_ = m;
      k_ = SPRING_K[m];
      c_ = 2 * SPRING_ZETA[m] * sqrtf(k_);
      if (m == SHOCKED) blinkStart_ = -10, nextBlink_ = t_ + 1.2f;  // wide eyed
    }
    float tgt[PARAM_COUNT];
    targets(m, tgt);
    float h = dt / 2;
    for (int s = 0; s < 2; s++)
      for (int i = 0; i < PARAM_COUNT; i++) {
        vel_[i] += (k_ * (tgt[i] - cur_[i]) - c_ * vel_[i]) * h;
        cur_[i] += vel_[i] * h;
      }
    // clamp unit params so overshoot never inverts a shape
    cur_[LID] = clampf(cur_[LID], 0, 1);
    cur_[HAPPY] = clampf(cur_[HAPPY], 0, 1);
    cur_[SQUEEZE] = clampf(cur_[SQUEEZE], 0, 1);
    cur_[BROW] = clampf(cur_[BROW], 0, 1.2f);
    cur_[MOUTH_OPEN] = clampf(cur_[MOUTH_OPEN], 0, 1.3f);
    cur_[SPEED_FX] = clampf(cur_[SPEED_FX], 0, 1);
    if (cur_[EYE_R] < 2) cur_[EYE_R] = 2;
    if (cur_[MOUTH_W] < 4) cur_[MOUTH_W] = 4;

    updateBlink(m);
    updateGaze(m, dt);
    if (cur_[LID] > 0.85f) zClock_ += dt; else zClock_ = 0;
  }

  void render() {
    const float* p = cur_;
    fg_ = light_ ? 0 : 1;
    d_.fillScreen(light_ ? 1 : 0);
    bool asleep = p[LID] > 0.8f;
    float bob = asleep ? sinf(t_ * 6.2832f / 5.0f) * 1.3f : sinf(t_ * 6.2832f / 4.0f) * 0.7f;
    float fx = p[SPEED_FX];
    float cx = 64 + p[LOOK_X] + fx * sinf(t_ * 53) * 1.2f;
    float cy = p[LOOK_Y] + bob + fx * sinf(t_ * 41 + 1) * 0.8f;

    if (fx > 0.02f) speedLines(fx);

    float lid = p[LID] + (1 - p[LID]) * blinkAmt_;
    for (int side = -1; side <= 1; side += 2) {
      float ex = cx + side * p[EYE_SPREAD] + gx_;
      float ey = p[EYE_Y] + cy + gy_;
      float r = p[EYE_R] * (1 + side * p[EYE_SKEW] * 0.3f);
      float sq = p[SQUEEZE];
      float er = r * (1 - sq);
      if (er > 1.2f) eye(ex, ey, er, er, lid, p[HAPPY]);
      if (sq > 0.05f) {
        float k = r * 0.95f * fminf(1, sq * 1.4f);
        float tipX = ex - side * 0.5f * k;
        capsule(tipX, ey, ex + side * 0.5f * k, ey - 0.65f * k, 1.9f);
        capsule(tipX, ey, ex + side * 0.5f * k, ey + 0.65f * k, 1.9f);
      }
      if (p[BROW] > 0.08f) {
        float L = r * 1.25f * p[BROW];
        float by = ey - r - 3.5f - p[BROW] * 1.5f;
        float inner = -p[BROW_TILT] * 3.5f, outer = p[BROW_TILT] * 3.5f;
        capsule(ex - side * L / 2, by + inner, ex + side * L / 2, by + outer, 1.6f);
      }
    }
    mouth(cx + gx_ * 0.4f, p[MOUTH_Y] + cy + gy_ * 0.4f, p[MOUTH_W], p[MOUTH_CURVE],
          p[MOUTH_OPEN], p[MOUTH_GRIN], p[MOUTH_THICK]);

    if (p[LID] > 0.85f) {  // floating z's from above the right eye
      float ox = cx + p[EYE_SPREAD] + 6, oy = p[EYE_Y] + cy - 8;
      for (int i = 0; i < 3; i++) {
        float ph = zClock_ / 2.4f + i / 3.0f - 0.4f;
        if (ph < 0) continue;
        ph -= floorf(ph);
        if (ph < 0.06f || ph > 0.94f) continue;
        float s = 3 + ph * 4;
        zee(ox + ph * 10 - s / 2, oy - ph * 22 - s, s);
      }
    }
  }

 private:
  enum StepKind : uint8_t { STEP_MOOD, STEP_FLASH, STEP_BLINK };
  struct Step { float at; uint8_t kind; uint8_t mood; uint16_t ms; };
  static const uint32_t DROWSY_MS = 60000, SLEEP_MS = 30000;

  D& d_;
  float cur_[PARAM_COUNT], vel_[PARAM_COUNT];
  float k_ = 170, c_ = 20;
  Mood base_ = NEUTRAL, flash_ = MOOD_COUNT, springMood_ = MOOD_COUNT;
  float flashUntil_ = 0, t_ = 0, lean_ = 0, shockG_ = 0.25f;
  uint32_t lastMs_ = 0, lastFrameMs_ = 0;
  uint16_t frameMs_ = 33;
  bool light_ = true;
  uint8_t fg_ = 0;
  uint32_t rng_ = 1;
  float blinkStart_ = -10, blinkSlow_ = 1, nextBlink_ = 2, blinkAmt_ = 0;
  float gx_ = 0, gy_ = 0, gvx_ = 0, gvy_ = 0, gtx_ = 0, gty_ = 0, nextGaze_ = 1;
  float zClock_ = 0;
  Step queue_[6];
  uint8_t queueLen_ = 0;
  uint32_t driveLastMs_ = 0, lastActiveMs_ = 0, calmSinceMs_ = 0;
  float holdAcc_ = 0, holdBrk_ = 0, prevLon_ = 0;

  static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
  static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

  float rnd() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (rng_ >> 8) / 16777216.0f;
  }

  void push(float delay, uint8_t kind, uint8_t m, uint16_t ms = 0) {
    if (queueLen_ < 6) queue_[queueLen_++] = Step{ t_ + delay, kind, m, ms };
  }
  void runQueue() {
    uint8_t w = 0;
    for (uint8_t i = 0; i < queueLen_; i++) {
      Step s = queue_[i];
      if (t_ < s.at) { queue_[w++] = s; continue; }
      if (s.kind == STEP_MOOD) base_ = (Mood)s.mood;
      else if (s.kind == STEP_FLASH) flash((Mood)s.mood, s.ms);
      else startBlink(1.0f);
    }
    queueLen_ = w;
  }

  void targets(Mood m, float* o) const {
    for (int i = 0; i < PARAM_COUNT; i++) o[i] = PRESETS[m][i];
    if (m == ASLEEP || m == SLEEPY) return;
    float a = fabsf(lean_);
    o[LOOK_X] += lean_ * 20;
    o[EYE_SPREAD] -= a * 16;
    o[EYE_SKEW] += lean_ * 0.35f;
    if (m == SHOCKED || m == BRAKING || m == SPEEDING) return;
    // gentle turn: having fun; hard turn: hanging on for dear life
    float fun = clampf(a / 0.35f, 0, 1) * (1 - clampf((a - 0.55f) / 0.25f, 0, 1));
    float scared = clampf((a - 0.55f) / 0.25f, 0, 1);
    o[MOUTH_W] = lerpf(o[MOUTH_W], 22, fun);
    o[MOUTH_CURVE] = lerpf(o[MOUTH_CURVE], 0, fun);
    o[MOUTH_GRIN] = lerpf(o[MOUTH_GRIN], 6, fun);
    o[HAPPY] = lerpf(o[HAPPY], 0, a);
    o[MOUTH_W] = lerpf(o[MOUTH_W], 12, scared);
    o[MOUTH_OPEN] = lerpf(o[MOUTH_OPEN], 1, scared);
    o[MOUTH_GRIN] = lerpf(o[MOUTH_GRIN], 0, scared);
    o[MOUTH_CURVE] = lerpf(o[MOUTH_CURVE], 0, scared);
    o[BROW] = lerpf(o[BROW], 1, scared);
    o[BROW_TILT] = lerpf(o[BROW_TILT], 0.7f, scared);
    o[EYE_R] = lerpf(o[EYE_R], 13, scared);
    o[EYE_Y] = lerpf(o[EYE_Y], 25, scared);
    o[MOUTH_Y] = lerpf(o[MOUTH_Y], 49, scared);
  }

  void startBlink(float slow) { blinkStart_ = t_; blinkSlow_ = slow; }

  void updateBlink(Mood m) {
    bool allowed = m != ASLEEP && m != SHOCKED;
    if (allowed && t_ >= nextBlink_) {
      startBlink(m == SLEEPY ? 3.0f : 1.0f);
      bool dbl = rnd() < 0.22f;
      nextBlink_ = t_ + (dbl ? 0.32f : 2.2f + rnd() * 4.0f);
    }
    float c = 0.07f * blinkSlow_, h = 0.04f * blinkSlow_, o = 0.12f * blinkSlow_;
    float p = t_ - blinkStart_;
    if (p < 0 || p > c + h + o) blinkAmt_ = 0;
    else if (p < c) { float u = p / c; blinkAmt_ = u * u; }
    else if (p < c + h) blinkAmt_ = 1;
    else { float u = 1 - (p - c - h) / o; blinkAmt_ = u * u * (3 - 2 * u); }
  }

  void updateGaze(Mood m, float dt) {
    bool idle = m == NEUTRAL || m == CONTENT || m == JOY || m == SLEEPY;
    if (!idle) { gtx_ = 0; gty_ = 0; }
    else if (t_ >= nextGaze_) {
      if (rnd() < 0.4f) { gtx_ = 0; gty_ = 0; }
      else { gtx_ = (rnd() * 2 - 1) * 5; gty_ = (rnd() * 2 - 1) * 2; }
      nextGaze_ = t_ + 0.8f + rnd() * 2.7f;
    }
    const float k = 700, c = 2 * 0.85f * 26.458f;
    gvx_ += (k * (gtx_ - gx_) - c * gvx_) * dt; gx_ += gvx_ * dt;
    gvy_ += (k * (gty_ - gy_) - c * gvy_) * dt; gy_ += gvy_ * dt;
  }

  // ---- raster helpers (pixel centres at +0.5) ----
  void hspan(int y, float xa, float xb) {
    if (y < 0 || y >= 64) return;
    int a = (int)ceilf(xa - 0.5f), b = (int)floorf(xb - 0.5f);
    if (a < 0) a = 0;
    if (b > 127) b = 127;
    if (b >= a) d_.drawFastHLine(a, y, b - a + 1, fg_);
  }
  void vspan(int x, float ya, float yb) {
    if (x < 0 || x >= 128) return;
    int a = (int)ceilf(ya - 0.5f), b = (int)floorf(yb - 0.5f);
    if (a < 0) a = 0;
    if (b > 63) b = 63;
    if (b >= a) d_.drawFastVLine(x, a, b - a + 1, fg_);
  }

  // Ellipse that squashes into a rounded dash as the lid closes; `happy`
  // carves the bottom out into a ^ arc.
  void eye(float ex, float ey, float rx, float ry, float lid, float happy) {
    float ryE = ry * (1 - lid);
    if (ryE < 2.6f) ryE = 2.6f;
    float cy = ey + (ry - ryE) * 0.35f;
    float rxE = rx * (1 + 0.12f * lid);
    float n = 2 + 4 * lid;
    float cutCy = cy + ryE * (2.0f - 1.4f * happy), cutRx = rxE * 0.92f, cutRy = ryE * 0.95f;
    int y0 = (int)floorf(cy - ryE), y1 = (int)ceilf(cy + ryE);
    for (int y = y0; y <= y1; y++) {
      float a = fabsf((y + 0.5f - cy) / ryE);
      if (a >= 1) continue;
      float hw = rxE * powf(1 - powf(a, n), 1 / n);
      if (happy > 0.01f) {
        float cd = (y + 0.5f - cutCy) / cutRy;
        if (fabsf(cd) < 1) {
          float hc = cutRx * sqrtf(1 - cd * cd);
          if (hc < hw) { hspan(y, ex - hw, ex - hc); hspan(y, ex + hc, ex + hw); }
          continue;
        }
      }
      hspan(y, ex - hw, ex + hw);
    }
  }

  // Thick line with round caps.
  void capsule(float ax, float ay, float bx, float by, float r) {
    float dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy;
    int x0 = (int)floorf(fminf(ax, bx) - r), x1 = (int)ceilf(fmaxf(ax, bx) + r);
    int y0 = (int)floorf(fminf(ay, by) - r), y1 = (int)ceilf(fmaxf(ay, by) + r);
    for (int y = y0; y <= y1; y++) {
      float py = y + 0.5f;
      int first = 999, last = -999;
      for (int x = x0; x <= x1; x++) {
        float px = x + 0.5f;
        float t = L2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / L2 : 0;
        t = clampf(t, 0, 1);
        float qx = ax + t * dx - px, qy = ay + t * dy - py;
        if (qx * qx + qy * qy <= r * r) { if (x < first) first = x; last = x; }
      }
      if (last >= first) hspan(y, first + 0.5f, last + 0.5f);
    }
  }

  // Curve > 0 smiles, open > 0 makes an O, grin bows the bottom lip into a D.
  void mouth(float cx, float cy, float w, float curve, float open, float grin, float thick) {
    float hw = w / 2;
    int x0 = (int)floorf(cx - hw), x1 = (int)ceilf(cx + hw);
    for (int x = x0; x <= x1; x++) {
      float dx = x + 0.5f - cx;
      float t = dx / hw;
      if (fabsf(t) >= 1) continue;
      float u = 1 - t * t, s = sqrtf(u);
      float base = cy + curve * (u - 0.5f);
      float e = hw - fabsf(dx), ht = thick / 2;
      if (e < ht) ht = sqrtf(fmaxf(0, ht * ht - (ht - e) * (ht - e)));
      float oh = open * hw * 0.75f * s;  // keeps an O round at any width
      vspan(x, base - ht - oh, base + ht + oh + grin * s);
    }
  }

  void zee(float x, float y, float s) {
    capsule(x, y, x + s, y, 0.9f);
    capsule(x + s, y, x, y + s, 0.9f);
    capsule(x, y + s, x + s, y + s, 0.9f);
  }

  // Streaks rushing outward from behind the face.
  void speedLines(float fx) {
    int n = (int)(fx * 12 + 0.5f);
    for (int i = 0; i < n; i++) {
      float ang = i * 0.5236f + 0.26f + ((i * 7) % 5) * 0.08f;
      float ph = t_ * 1.6f + ((i * 37) % 11) / 11.0f;
      ph -= floorf(ph);
      float r0 = 30 + ph * 60, len = 4 + ph * 14;
      float ca = cosf(ang) * 1.6f, sa = sinf(ang);
      capsule(64 + ca * r0, 32 + sa * r0 * 0.9f, 64 + ca * (r0 + len), 32 + sa * (r0 + len) * 0.9f, 1.0f);
    }
  }
};

}  // namespace carface
