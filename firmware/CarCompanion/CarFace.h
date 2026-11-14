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
// drive() is optional: call setMood()/flash()/setLean()/wake()/playIdle() yourself
// if you'd rather keep your own detection logic.
#pragma once
#include <math.h>
#include <stdint.h>

namespace carface {

enum Mood : uint8_t {
  NEUTRAL, CONTENT, JOY, SHOCKED, SLEEPY, ASLEEP, SPEEDING, BRAKING,
  RATTLED,  // rough road: squinting, gritted teeth, shaken by every bump
  OOF,      // one big pothole
  MUSIC,    // idle: eyes shut, bobbing to a beat, notes floating up
  WHISTLE,  // idle: bored, looking away, whistling, the odd sigh
  TURBO,    // idle: "phew", gets flushed round into a spinning turbo wheel and back; queasy, then worn out
  DOZE,     // idle: yawns, nods off, jolts awake, nods off again with z's, then starts awake
  SNEEZE,   // idle: nose twitch, two false starts, ACHOO
  FLY,      // idle: watches a fly, goes cross-eyed when it lands on its nose, blows it away
  WIPERS,   // idle: raindrops, an annoyed look, a wiper sweeps its face clean
  HICCUP,   // idle: hiccups, holds its breath with puffed cheeks, one more sneaks out
  REV,      // idle: bored, makes engine noises, then checks whether anyone saw
  PEEK,     // idle: slides off the bottom, peeks over the edge, pops up grinning
  MIRROR,   // idle: checks left and right, then a smug raised brow and a wink
  DREAM,    // idle: daydreams of a turbo in a thought bubble until it pops
  MOOD_COUNT
};

enum Param : uint8_t {
  LOOK_X, LOOK_Y, EYE_SPREAD, EYE_Y, EYE_R, EYE_SKEW, LID, HAPPY, SQUEEZE,
  BROW, BROW_TILT, MOUTH_W, MOUTH_CURVE, MOUTH_OPEN, MOUTH_GRIN, MOUTH_THICK,
  MOUTH_Y, SPEED_FX, TEETH, MOUTH_X, BEAT, NOTES, PARAM_COUNT
};

// Target face for each mood (pixels on a 128x64 screen unless noted).
static const float PRESETS[MOOD_COUNT][PARAM_COUNT] = {
  // lookX lookY spread eyeY eyeR skew  lid  happy sqz  brow tilt  mW  curve open grin thick mY  fx  teeth mX  beat notes
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    24,  0,   0,   0,   5,   47,  0,  0,  0,  0,   0 }, // NEUTRAL
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    32,  3,   0,   0,   5,   47,  0,  0,  0,  0,   0 }, // CONTENT
  {  0,    1,    40,    24,  12,  0,    0,   1,    0,   0,   0,    24,  0,   0,   5,   4,   45,  0,  0,  0,  0,   0 }, // JOY
  {  0,    0,    42,    24,  15,  0,    0,   0,    0,   0.6, 0,    15,  0,   1,   0,   4,   49,  0,  0,  0,  0,   0 }, // SHOCKED
  {  0,    2,    41,    24,  11,  0,    0.62,0,    0,   0,   0,    20,  0,   0,   0,   4,   48,  0,  0,  0,  0,   0 }, // SLEEPY
  {  0,    3,    41,    26,  11,  0,    1,   0,    0,   0,   0,    28,  0,   0,   0,   4,   48,  0,  0,  0,  0,   0 }, // ASLEEP
  {  0,    0,    33,    24,   5,  0,    0,   0,    0,   0.8, 0.6,  10,  0,   1,   0,   3,   46,  1,  0,  0,  0,   0 }, // SPEEDING
  {  0,    3,    41,    23,  11,  0,    0,   0,    1,   1,  -0.7,  22, -4,   0,   0,   4,   48,  0,  0,  0,  0,   0 }, // BRAKING
  {  0,    1,    40,    23,  11,  0.25, 0.45,0,    0,   1,   0.45, 30,  0,   0,   0,   4,   48,  0,  1,  0,  0,   0 }, // RATTLED
  {  0,    0,    40,    23,  12,  0,    0,   0,    1,   0.9, 0.6,  14,  0,   0.9, 0,   4,   49,  0,  0,  0,  0,   0 }, // OOF
  {  0,    1,    40,    24,  12,  0,    0,   1,    0,   0,   0,    20,  2.5, 0,   0,   4,   46,  0,  0,  0,  1,   1 }, // MUSIC
  {  0,    0,    41,    22,  11,  0,    0.45,0,    0,   0,   0,     8,  0,   1,   0,   3,   47,  0,  0,  6,  0,   1 }, // WHISTLE
  {  0,    1,    41,    23,  11,  0,    1,   0,    0,   0.7, 0.6,   9,  0,   1,   0,   3,   48,  0,  0,  0,  0,   0 }, // TURBO (the "phew")
  {  0,    1,    41,    23,  11,  0,    0.5, 0,    0,   0,   0,    18,  0,   0,   0,   4,   48,  0,  0,  0,  0,   0 }, // DOZE
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    32,  3,   0,   0,   5,   47,  0,  0,  0,  0,   0 }, // SNEEZE (scripted, see actTargets)
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    32,  3,   0,   0,   5,   47,  0,  0,  0,  0,   0 }, // FLY (scripted, see actTargets)
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    32,  3,   0,   0,   5,   47,  0,  0,  0,  0,   0 }, // WIPERS (scripted, see actTargets)
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    32,  3,   0,   0,   5,   47,  0,  0,  0,  0,   0 }, // HICCUP (scripted, see actTargets)
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    32,  3,   0,   0,   5,   47,  0,  0,  0,  0,   0 }, // REV (scripted, see actTargets)
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    32,  3,   0,   0,   5,   47,  0,  0,  0,  0,   0 }, // PEEK (scripted, see actTargets)
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    32,  3,   0,   0,   5,   47,  0,  0,  0,  0,   0 }, // MIRROR (scripted, see actTargets)
  {  0,    0,    41,    22,  11,  0,    0,   0,    0,   0,   0,    32,  3,   0,   0,   5,   47,  0,  0,  0,  0,   0 }, // DREAM (scripted, see actTargets)
};

// Spring stiffness and damping ratio per mood: low damping = bouncy pop.
static const float SPRING_K[MOOD_COUNT]    = { 170, 140, 220, 600,  20,  12, 300, 450, 380, 700, 120, 110, 300, 200,
                                               300, 220, 220, 260, 260, 220, 220, 200 };
static const float SPRING_ZETA[MOOD_COUNT] = { 0.8, 0.8, 0.6, 0.42, 1.0, 1.0, 0.55, 0.5, 0.45, 0.4, 0.8, 0.85, 0.6, 0.6,
                                               0.55, 0.7, 0.7, 0.6, 0.6, 0.7, 0.7, 0.75 };

// How long each idle act runs, MUSIC onwards (ms).
static const uint16_t ACT_MS[MOOD_COUNT - MUSIC] = { 8000, 7000, 11400, 10000, 5000, 9000, 7000, 7200, 7000, 7000, 6000, 8500 };
static const float HIC[4] = { 0.6f, 1.7f, 2.8f, 6.2f };            // when the hiccups come
static const float DROPX[6] = { 46, 86, 64, 121, 7, 30 };         // where the raindrops land
static const float DROPY[6] = { 8, 13, 27, 42, 46, 56 };

static const char* const MOOD_NAMES[MOOD_COUNT] = {
  "neutral", "content", "joy", "shocked", "sleepy", "asleep", "speeding", "braking",
  "rattled", "oof", "music", "whistle", "turbo", "doze",
  "sneeze", "fly", "wipers", "hiccup", "rev", "peek", "mirror", "dream"
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
    shX_ = shY_ = shVx_ = shVy_ = 0; rough_ = 0; roughOn_ = roughOff_ = 0;
    moodT_ = 0; gazeHoldUntil_ = 0; idleArmed_ = false; nextIdleMs_ = 0; lastIdle_ = MOOD_COUNT;
    boot();
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

  // Road feel from the vertical axis, once per sensor sample.
  //   joltG: vertical g with gravity and slow drift removed (a bump is a spike)
  //   roughnessG: average size of those jolts over the last half second or so
  // Every jolt knocks the face around; sustained roughness makes it RATTLED,
  // and one big hit on a smooth road gets an OOF.
  void road(float joltG, float roughnessG, uint32_t nowMs) {
    rough_ = roughnessG;
    float kick = clampf(joltG, -1.5f, 1.5f);
    if (fabsf(kick) > 0.04f) {
      shVy_ += kick * 260;                        // bumps throw the face up/down
      shVx_ += (rnd() * 2 - 1) * fabsf(kick) * 140;  // and rattle it sideways a bit
    }
    if (base_ == ASLEEP || base_ == SLEEPY) return;
    if (fabsf(joltG) > OOF_G && rough_ < ROUGH_ON_G && (flash_ != OOF)) { flash(OOF, 650); blink(); }
    (void)nowMs;
  }
  float roughness() const { return rough_; }
  Mood mood() const { return flash_ < MOOD_COUNT ? flash_ : base_; }
  Mood baseMood() const { return base_; }
  const float* params() const { return cur_; }

  // Power-on, 20 s: asleep with z's for 10 s, wakes with a start, looks
  // around, then smiles.
  void boot() {
    queueLen_ = 0; flash_ = MOOD_COUNT;
    base_ = ASLEEP;
    push(10.0f, STEP_FLASH, SHOCKED, 1300);
    push(10.0f, STEP_MOOD, NEUTRAL);
    push(11.4f, STEP_GAZE, 0, 1300, -10, 0);
    push(12.7f, STEP_BLINK, 0);
    push(12.8f, STEP_GAZE, 0, 1300, 10, 0);
    push(14.1f, STEP_GAZE, 0, 1000, 0, -4);
    push(15.1f, STEP_BLINK, 0);
    push(15.2f, STEP_GAZE, 0, 1000, -6, 2);
    push(16.2f, STEP_GAZE, 0, 800, 0, 0);
    push(17.0f, STEP_FLASH, JOY, 2400);
    push(17.0f, STEP_MOOD, CONTENT);
  }

  // Play one idle act now (any mood from MUSIC onwards), then return to the base mood.
  void playIdle(Mood m) {
    if (!isIdleAct(m)) return;
    flash(m, ACT_MS[m - MUSIC]);
    lastIdle_ = m;
  }

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
    float act = fmaxf(fmaxf(fabsf(forwardG), fabsf(lateralG)), rough_);
    if (act > 0.06f) lastActiveMs_ = nowMs;
    if (act > 0.25f) calmSinceMs_ = nowMs;
    // any real motion cuts an idle act short
    if (act > 0.2f && isIdleAct(flash_)) flash_ = MOOD_COUNT;

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

    // Rough road: switch on after 0.4 s of shaking, off after 1 s of smooth.
    roughOn_ = rough_ > ROUGH_ON_G ? roughOn_ + dt : 0;
    roughOff_ = rough_ < ROUGH_OFF_G ? roughOff_ + dt : 0;
    if (base_ != RATTLED && roughOn_ > 0.4f) base_ = RATTLED;
    if (base_ == RATTLED) {
      calmSinceMs_ = nowMs;
      if (roughOff_ > 1.0f) base_ = NEUTRAL;
      else return;
    }

    uint32_t calm = nowMs - calmSinceMs_;
    if (base_ == NEUTRAL && calm > 6000) base_ = CONTENT;
    bool still = nowMs - lastActiveMs_ > STILL_MS;
    if (base_ == CONTENT && calm > 25000 && flash_ == MOOD_COUNT && !still) { flash(JOY, 1600); calmSinceMs_ = nowMs; }  // smooth driving!
    if (nowMs - lastActiveMs_ > DROWSY_MS) { base_ = SLEEPY; if (isIdleAct(flash_)) flash_ = MOOD_COUNT; }

    // Idle acts: once things have been still for a while, entertain itself
    // every 20-40 s until it gets drowsy.
    bool freeNow = still && queueLen_ == 0 && flash_ == MOOD_COUNT && (base_ == CONTENT || base_ == NEUTRAL);
    if (!still) idleArmed_ = false;
    else if (freeNow && !idleArmed_) { idleArmed_ = true; nextIdleMs_ = nowMs + 3000 + (uint32_t)(rnd() * 4000); }
    else if (freeNow && (int32_t)(nowMs - nextIdleMs_) >= 0) {
      Mood m = (Mood)(MUSIC + (int)(rnd() * IDLE_ACTS) % IDLE_ACTS);
      if (m == lastIdle_) m = (Mood)(MUSIC + (m - MUSIC + 1) % IDLE_ACTS);
      playIdle(m);
      nextIdleMs_ = nowMs + 20000 + (uint32_t)(rnd() * 20000);
    }
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
      moodT_ = 0;
      k_ = SPRING_K[m];
      c_ = 2 * SPRING_ZETA[m] * sqrtf(k_);
      if (m == SHOCKED) blinkStart_ = -10, nextBlink_ = t_ + 1.2f;  // wide eyed
    }
    else moodT_ += dt;
    float tgt[PARAM_COUNT];
    targets(m, tgt);
    // All springs move in fixed steps of at most 10 ms, so a slow frame
    // (60-100 ms on a busy XIAO) can never make them overshoot and run away.
    int n = (int)ceilf(dt / 0.01f);
    if (n < 1) n = 1;
    if (n > 12) n = 12;
    float h = dt / n;
    updateGazeTarget(m);
    const float gk = 700, gc = 2 * 0.85f * 26.458f;
    const float sk = 900, sc = 2 * 0.22f * 30.0f;
    for (int s = 0; s < n; s++) {
      for (int i = 0; i < PARAM_COUNT; i++) {
        vel_[i] += (k_ * (tgt[i] - cur_[i]) - c_ * vel_[i]) * h;
        cur_[i] += vel_[i] * h;
      }
      gvx_ += (gk * (gtx_ - gx_) - gc * gvx_) * h; gx_ += gvx_ * h;
      gvy_ += (gk * (gty_ - gy_) - gc * gvy_) * h; gy_ += gvy_ * h;
      shVx_ += (-sk * shX_ - sc * shVx_) * h; shX_ += shVx_ * h;
      shVy_ += (-sk * shY_ - sc * shVy_) * h; shY_ += shVy_ * h;
    }
    // Belt and braces: a value that is not finite snaps back to its target.
    for (int i = 0; i < PARAM_COUNT; i++)
      if (!(fabsf(cur_[i]) < 1000)) { cur_[i] = tgt[i]; vel_[i] = 0; }
    if (!(fabsf(gx_) < 100) || !(fabsf(gy_) < 100)) gx_ = gy_ = gvx_ = gvy_ = 0;
    if (!(fabsf(shX_) < 100) || !(fabsf(shY_) < 100)) shX_ = shY_ = shVx_ = shVy_ = 0;
    // clamp unit params so overshoot never inverts a shape
    cur_[LID] = clampf(cur_[LID], 0, 1);
    cur_[HAPPY] = clampf(cur_[HAPPY], 0, 1);
    cur_[SQUEEZE] = clampf(cur_[SQUEEZE], 0, 1);
    cur_[BROW] = clampf(cur_[BROW], 0, 1.2f);
    cur_[MOUTH_OPEN] = clampf(cur_[MOUTH_OPEN], 0, 1.3f);
    cur_[SPEED_FX] = clampf(cur_[SPEED_FX], 0, 1);
    cur_[TEETH] = clampf(cur_[TEETH], 0, 1);
    cur_[BEAT] = clampf(cur_[BEAT], 0, 1);
    cur_[NOTES] = clampf(cur_[NOTES], 0, 1);
    if (cur_[EYE_R] < 2) cur_[EYE_R] = 2;
    if (cur_[MOUTH_W] < 4) cur_[MOUTH_W] = 4;

    // Shake spring (kicked by road()) is integrated above with the others.
    shX_ = clampf(shX_, -6, 6); shY_ = clampf(shY_, -7, 7);
    gx_ = clampf(gx_, -12, 12); gy_ = clampf(gy_, -5, 5);

    updateBlink(m);
    if (cur_[LID] > 0.85f) zClock_ += dt; else zClock_ = 0;
  }

  void render() {
    const float* p = cur_;
    fg_ = light_ ? 0 : 1;
    d_.fillScreen(light_ ? 1 : 0);
    bool asleep = p[LID] > 0.8f;
    float bob = asleep ? sinf(t_ * 6.2832f / 5.0f) * 1.3f : sinf(t_ * 6.2832f / 4.0f) * 0.7f;
    float fx = p[SPEED_FX];
    float cx = 64 + p[LOOK_X] + fx * sinf(t_ * 53) * 1.2f + shX_;
    float cy = p[LOOK_Y] + bob + fx * sinf(t_ * 41 + 1) * 0.8f + shY_;
    // Music: nod down on every beat (100 BPM) and rock side to side.
    float beat = p[BEAT], tilt = 0;
    if (beat > 0.02f) {
      cy += beat * 3.0f * fabsf(sinf(t_ * 5.236f));
      tilt = beat * sinf(t_ * 2.618f);
    }

    if (fx > 0.02f) speedLines(fx);

    Mood md = mood();
    Fx f; float atau = md >= SNEEZE ? actTau(md, moodT_) : 0;
    actFx(md, atau, f);
    cx += f.ox; cy += f.oy;
    if (md == TURBO) {
      float morph, theta, tau = turboTau(moodT_);
      turboState(tau, morph, theta);
      // the "phew" before, and a worn-out sigh after: a puff of breath
      float u = tau < 5 ? (tau - 0.15f) / 0.65f : (tau - 10.35f) / 0.65f;
      if (u > 0 && u < 1) {
        float px = cx + p[MOUTH_X] + 9 + u * 16, py = p[MOUTH_Y] + cy;
        for (int i = -1; i <= 1; i++)
          capsule(px, py + i * (3 + u * 3), px + 5 - (i ? 1 : 0), py + i * (4 + u * 4), 0.8f);
      }
      if (morph > 0.001f) { turbo(cx, 32 + cy, morph, theta); return; }
    }

    float lid = p[LID] + (1 - p[LID]) * blinkAmt_;
    for (int side = -1; side <= 1; side += 2) {
      float ex = cx + side * (p[EYE_SPREAD] - f.cross) + gx_;
      float ey = p[EYE_Y] + cy + gy_ + side * tilt * 2.5f;
      float r = p[EYE_R] * (1 + side * p[EYE_SKEW] * 0.3f);
      float sq = p[SQUEEZE];
      float er = r * (1 - sq);
      if (er > 1.2f) eye(ex, ey, er, er, side > 0 ? lid + (1 - lid) * f.winkR : lid, p[HAPPY]);
      if (sq > 0.05f) {
        float k = r * 0.95f * fminf(1, sq * 1.4f);
        float tipX = ex - side * 0.5f * k;
        capsule(tipX, ey, ex + side * 0.5f * k, ey - 0.65f * k, 1.9f);
        capsule(tipX, ey, ex + side * 0.5f * k, ey + 0.65f * k, 1.9f);
      }
      if (p[BROW] > 0.08f) {
        float L = r * 1.25f * p[BROW];
        float by = ey - r - 3.5f - p[BROW] * 1.5f - (side > 0 ? f.browR : 0);
        float inner = -p[BROW_TILT] * 3.5f, outer = p[BROW_TILT] * 3.5f;
        capsule(ex - side * L / 2, by + inner, ex + side * L / 2, by + outer, 1.6f);
      }
    }
    float teeth = p[TEETH];
    float mx = cx + gx_ * 0.4f + p[MOUTH_X] - tilt * 3, my = p[MOUTH_Y] + cy + gy_ * 0.4f;
    if (teeth > 0.15f) {
      // Gritted teeth, the grimace wobbling with the shaking.
      teethMouth(mx, my - shY_ * 0.35f, p[MOUTH_W], 2 + 6 * teeth, shY_ * 0.5f + sinf(t_ * 31) * 0.8f * teeth);
    } else {
      mouth(mx, my, p[MOUTH_W], p[MOUTH_CURVE], p[MOUTH_OPEN], p[MOUTH_GRIN], p[MOUTH_THICK]);
    }

    if (p[NOTES] > 0.05f) {  // music notes: from the mouth when whistling, else between the eyes
      bool wh = md == WHISTLE;
      int n = (int)(p[NOTES] * 3 + 0.5f);
      for (int i = 0; i < n; i++) {
        float ph = t_ / 1.8f + i / 3.0f;
        ph -= floorf(ph);
        if (ph < 0.08f || ph > 0.92f) continue;
        float sway = sinf(ph * 6.2832f + i * 2.1f) * 3;
        if (wh) note(mx + 6 + ph * 8 + sway * 0.4f, my - 5 - ph * 22, i & 1);
        else note(cx - 12 + i * 12 + sway, 40 + cy - ph * 26, i & 1);
      }
    }

    if (p[LID] > 0.85f && md != TURBO && md < SNEEZE) {  // floating z's from above the right eye
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
    if (md >= SNEEZE) actOverlay(md, atau, cx, cy, mx, my);
  }

 private:
  enum StepKind : uint8_t { STEP_MOOD, STEP_FLASH, STEP_BLINK, STEP_GAZE };
  struct Step { float at; uint8_t kind; uint8_t mood; uint16_t ms; int8_t gx, gy; };
  static const uint32_t STILL_MS = 8000, DROWSY_MS = 150000, SLEEP_MS = 30000;
  static constexpr float ROUGH_ON_G = 0.10f, ROUGH_OFF_G = 0.07f, OOF_G = 0.6f;

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
  Step queue_[12];
  uint8_t queueLen_ = 0;
  uint32_t driveLastMs_ = 0, lastActiveMs_ = 0, calmSinceMs_ = 0;
  float holdAcc_ = 0, holdBrk_ = 0, prevLon_ = 0;
  float shX_ = 0, shY_ = 0, shVx_ = 0, shVy_ = 0;
  float rough_ = 0, roughOn_ = 0, roughOff_ = 0;
  float moodT_ = 0, gazeHoldUntil_ = 0;   // seconds in the current mood; scripted gaze
  bool idleArmed_ = false;
  uint32_t nextIdleMs_ = 0;
  Mood lastIdle_ = MOOD_COUNT;

  static const int IDLE_ACTS = MOOD_COUNT - MUSIC;
  static bool isIdleAct(Mood m) { return m >= MUSIC && m < MOOD_COUNT; }
  static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
  static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

  float rnd() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return (rng_ >> 8) / 16777216.0f;
  }

  void push(float delay, uint8_t kind, uint8_t m, uint16_t ms = 0, int8_t gx = 0, int8_t gy = 0) {
    if (queueLen_ < 12) queue_[queueLen_++] = Step{ t_ + delay, kind, m, ms, gx, gy };
  }
  void runQueue() {
    uint8_t w = 0;
    for (uint8_t i = 0; i < queueLen_; i++) {
      Step s = queue_[i];
      if (t_ < s.at) { queue_[w++] = s; continue; }
      if (s.kind == STEP_MOOD) base_ = (Mood)s.mood;
      else if (s.kind == STEP_FLASH) flash((Mood)s.mood, s.ms);
      else if (s.kind == STEP_GAZE) { gtx_ = s.gx; gty_ = s.gy; gazeHoldUntil_ = t_ + s.ms / 1000.0f; nextGaze_ = gazeHoldUntil_; }
      else startBlink(1.0f);
    }
    queueLen_ = w;
  }

  void targets(Mood m, float* o) const {
    for (int i = 0; i < PARAM_COUNT; i++) o[i] = PRESETS[m][i];
    if (m == ASLEEP || m == SLEEPY) return;
    float tau = moodT_;
    if (m >= SNEEZE) { actTargets(m, actTau(m, tau), o); return; }
    if (m == TURBO) {
      tau = turboTau(tau);  // loops when held as the base mood
      if (tau > 0.65f) { o[LID] = 0; o[BROW] = 0; o[EYE_R] = 12; }  // eyes pop open before the flush
      if (tau > 7.4f) {  // queasy: wobbling, mismatched eyes, mouth sliding about
        float amp = 1 - 0.5f * clampf((tau - 7.4f) / 2.0f, 0, 1);
        float tired = ease((tau - 9.4f) / 0.6f);
        amp *= 1 - tired;
        o[EYE_SKEW] = 1.2f * sinf(tau * 11) * amp;
        o[LOOK_X] = 4 * sinf(tau * 7) * amp;
        o[MOUTH_X] = 3 * sinf(tau * 5 + 1) * amp;
        o[MOUTH_W] = 16; o[MOUTH_OPEN] = 0; o[MOUTH_CURVE] = -2.5f; o[MOUTH_THICK] = 4;
        o[EYE_R] = 11;
        // then worn out: heavy lids, drooping eyes, a small frown and a sigh
        o[LID] = 0.45f * tired; o[LOOK_Y] = lerpf(1, 4, tired); o[EYE_Y] = lerpf(23, 25, tired);
        o[BROW] = 0.7f * tired; o[BROW_TILT] = 0.6f;
        o[MOUTH_W] = lerpf(16, 12, tired); o[MOUTH_CURVE] = lerpf(-2.5f, -1.5f, tired);
        if (tau > 10.25f && tau < 10.95f) { o[MOUTH_OPEN] = 1; o[MOUTH_W] = 8; o[MOUTH_CURVE] = 0; o[MOUTH_THICK] = 3; }
      }
      return;
    }
    if (m == DOZE) {
      // 0-1.4 a yawn; 1.4-4.0 lids sink and the head droops; 4.0 jolts awake;
      // 4.7-8.6 nods off again, faster, and sleeps with z's; 8.6 starts awake,
      // shakes it off and smiles. Loops every 11 s when held as the base mood.
      if (tau >= 11.0f) tau -= 11.0f * floorf(tau / 11.0f);
      float lid, drop, wide = 0;
      if (tau < 1.4f) {
        float y = sinf(3.14159f * clampf((tau - 0.1f) / 1.3f, 0, 1));
        lid = lerpf(0.3f, 0.8f, y); drop = -2 * y;
        o[MOUTH_OPEN] = y; o[MOUTH_W] = lerpf(14, 17, y); o[BROW] = 0.6f * y;
      } else if (tau < 4.0f) {
        float u = ease((tau - 1.8f) / 2.2f);
        lid = lerpf(0.5f, 1, u); drop = 7 * u;
      } else if (tau < 4.7f) {
        lid = 0; drop = -2; wide = 1;
      } else if (tau < 8.6f) {
        float u = ease((tau - 4.7f) / 1.7f);
        lid = lerpf(0.35f, 1, u); drop = 8 * u; o[LOOK_X] = 3 * u; o[MOUTH_W] = lerpf(18, 24, u);
      } else if (tau < 9.4f) {
        lid = 0; drop = -1; wide = 1;
        o[LOOK_X] = 5 * sinf((tau - 8.7f) * 18) * clampf(1 - (tau - 8.7f) / 0.7f, 0, 1);
      } else {
        lid = 0; drop = 0; o[MOUTH_W] = 24; o[MOUTH_CURVE] = 2;
      }
      o[LID] = lid; o[LOOK_Y] = drop;
      if (wide > 0) { o[EYE_R] = 13; o[MOUTH_W] = 10; o[MOUTH_OPEN] = 0.6f; o[BROW] = 0.6f; }
      return;
    }
    if (m == MUSIC && fmodf(tau, 3.6f) > 2.4f && fmodf(tau, 3.6f) < 3.1f) {  // sings a note
      o[MOUTH_OPEN] = 0.8f; o[MOUTH_W] = 12; o[MOUTH_CURVE] = 0;
    }
    if (m == WHISTLE) {
      if (fmodf(tau, 5.0f) > 3.8f) {  // stops for a sigh
        o[MOUTH_OPEN] = 0; o[MOUTH_W] = 14; o[MOUTH_THICK] = 4; o[NOTES] = 0; o[LID] = 0.75f; o[LOOK_Y] = 2;
      } else {
        o[MOUTH_W] += 1.5f * sinf(tau * 8);  // the tune
      }
    }
    float a = fabsf(lean_);
    o[LOOK_X] += lean_ * 20;
    o[EYE_SPREAD] -= a * 16;
    o[EYE_SKEW] += lean_ * 0.35f;
    if (m == SHOCKED || m == BRAKING || m == SPEEDING || m == MUSIC || m == WHISTLE) return;
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
    bool allowed = m != ASLEEP && m != SHOCKED && m != TURBO && m != MUSIC && m != DOZE && m < SNEEZE;
    if (allowed && t_ >= nextBlink_) {
      startBlink(m == SLEEPY ? 3.0f : 1.0f);
      bool dbl = rnd() < 0.22f;
      nextBlink_ = t_ + (dbl ? 0.32f : 2.2f + rnd() * 4.0f);
    }
    float c = 0.07f * blinkSlow_, h = 0.04f * blinkSlow_, o = 0.12f * blinkSlow_;
    if (m >= SNEEZE) actBlink(m, actTau(m, moodT_));
    if (m == TURBO) {  // one slow, weary blink once the queasiness passes
      float tb = turboTau(moodT_) - 9.9f;
      if (tb >= 0 && tb < 0.7f) { blinkStart_ = t_ - tb; blinkSlow_ = 3.0f; }
    }
    float p = t_ - blinkStart_;
    if (p < 0 || p > c + h + o) blinkAmt_ = 0;
    else if (p < c) { float u = p / c; blinkAmt_ = u * u; }
    else if (p < c + h) blinkAmt_ = 1;
    else { float u = 1 - (p - c - h) / o; blinkAmt_ = u * u * (3 - 2 * u); }
  }

  // Where the eyes want to look. The spring that moves them is in step().
  void updateGazeTarget(Mood m) {
    if (t_ < gazeHoldUntil_) return;  // scripted look (boot sequence)
    if (m >= SNEEZE) { actGaze(m, actTau(m, moodT_)); return; }
    if (m == DOZE) { gtx_ = 0; gty_ = 0; return; }
    if (m == WHISTLE) {  // looking away, bored; eyes roll up for the sigh
      bool sigh = fmodf(moodT_, 5.0f) > 3.8f;
      gtx_ = 6 * sinf(moodT_ * 0.9f); gty_ = sigh ? -3.5f : -2;
      return;
    }
    if (m == TURBO && turboTau(moodT_) > 7.4f) {  // dizzy: eyes still going round, then drooping
      float tau = turboTau(moodT_);
      float amp = (1 - 0.5f * clampf((tau - 7.4f) / 2.0f, 0, 1)) * (1 - ease((tau - 9.4f) / 0.6f));
      gtx_ = 4 * cosf(tau * 8) * amp; gty_ = 2.5f * sinf(tau * 8) * amp + 2 * (1 - amp);
      return;
    }
    bool idle = m == NEUTRAL || m == CONTENT || m == JOY || m == SLEEPY || m == RATTLED;
    if (!idle) { gtx_ = 0; gty_ = 0; }
    else if (t_ >= nextGaze_) {
      if (rnd() < 0.4f) { gtx_ = 0; gty_ = 0; }
      else { gtx_ = (rnd() * 2 - 1) * 5; gty_ = (rnd() * 2 - 1) * 2; }
      nextGaze_ = t_ + 0.8f + rnd() * 2.7f;
    }
  }

  // ---- scripted idle acts (SNEEZE onwards) ----
  // Each is a pure function of the seconds since it started, so it plays the
  // same at any frame rate: actTargets() poses the face, actFx() moves the
  // whole face or one eye, actGaze() aims the eyes, actBlink() times blinks,
  // and actOverlay() draws the props (fly, raindrops, wiper, bubble...).
  struct Fx { float ox, oy, cross, winkR, browR; };
  static float win(float t, float a, float b, float r) { return ease((t - a) / r) * (1 - ease((t - b) / r)); }
  static float actTau(Mood m, float t) {  // loops, with a 1 s rest, when held as the base mood
    float per = ACT_MS[m - MUSIC] / 1000.0f + 1;
    return t >= per ? t - per * floorf(t / per) : t;
  }
  static void flyPos(float tau, float* q) {
    float x = 64 + 52 * cosf(1.3f * tau + 0.3f) + 5 * sinf(9 * tau) + 70 * (1 - ease(tau / 0.8f));
    float y = 22 + 15 * sinf(2.1f * tau) + 3 * sinf(11 * tau);
    if (tau >= 5.0f) {  // comes in to land on the nose
      float x5 = 64 + 52 * cosf(6.8f) + 5 * sinf(45.0f), y5 = 22 + 15 * sinf(10.5f) + 3 * sinf(55.0f);
      float u = ease((tau - 5.0f) / 0.8f);
      x = lerpf(x5, 64, u) + 16 * sinf(tau * 14) * u * (1 - u); y = lerpf(y5, 35, u) + 12 * cosf(tau * 17) * u * (1 - u);
    }
    if (tau >= 7.1f) {  // blown away
      float u = (tau - 7.1f) / 0.6f;
      x = 64 + 90 * u + 8 * sinf(u * 20) * u; y = 35 - 58 * u + 8 * cosf(u * 20) * u;
    }
    q[0] = x; q[1] = y;
  }
  static void actTargets(Mood m, float tau, float* o) {
    if (m == SNEEZE) {
      // nose twitches, two false starts, a third wind-up, then ACHOO; dazed blinks and a sniff
      float a3 = tau < 2.9f ? ease((tau - 2.3f) / 0.5f) : 0;
      float ah = fmaxf(fmaxf(0.45f * win(tau, 0.5f, 1.0f, 0.15f), 0.7f * win(tau, 1.4f, 1.95f, 0.15f)), a3);
      float boom = tau >= 2.9f ? 1 - ease((tau - 3.25f) / 0.3f) : 0;
      float tw = win(tau, 0.0f, 0.5f, 0.1f) + win(tau, 4.2f, 4.7f, 0.1f);
      o[MOUTH_X] = 2 * sinf(tau * 30) * tw;
      if (tau < 4.2f) { o[MOUTH_W] = 10 + 2 * ah; o[MOUTH_CURVE] = 0; o[MOUTH_THICK] = 3; }
      o[MOUTH_OPEN] = 1.1f * ah; o[LOOK_Y] = -6 * ah; o[LID] = 0.8f * ah;
      o[BROW] = 0.9f * fminf(1, ah * 3); o[BROW_TILT] = 0.6f;
      if (boom > 0) {
        o[LOOK_Y] = 4 * boom; o[SQUEEZE] = boom > 0.3f ? 1 : 0; o[MOUTH_W] = lerpf(12, 20, boom);
        o[MOUTH_OPEN] = boom; o[LID] = 0; o[BROW] = 0;
      }
    } else if (m == FLY) {
      // eyes follow a fly; it lands on the nose (cross-eyed), gets blown away
      float land = win(tau, 5.6f, 7.1f, 0.2f), blow = win(tau, 7.0f, 7.35f, 0.08f);
      if (tau < 7.9f) { o[MOUTH_W] = lerpf(16, 10, land); o[MOUTH_CURVE] = 0; o[MOUTH_THICK] = lerpf(4, 3, land); o[EYE_R] = 12; }
      o[BROW] = 0.7f * land; o[BROW_TILT] = 0.4f;
      if (blow > 0.1f) { o[MOUTH_OPEN] = 1; o[MOUTH_W] = 8; }
    } else if (m == WIPERS) {
      // raindrops land, it gets cross, a wiper sweeps over and back, relief
      float cross = win(tau, 1.6f, 3.1f, 0.2f), wipe = win(tau, 3.1f, 5.0f, 0.12f);
      float g1 = (tau - 3.45f) / 0.2f, g2 = (tau - 4.65f) / 0.2f;
      o[LOOK_X] = 7 * (expf(-g1 * g1) - expf(-g2 * g2));  // pushed along by the blade
      if (tau < 5.0f) { o[MOUTH_W] = lerpf(14, 18, cross); o[MOUTH_CURVE] = -2.5f * cross - wipe; o[MOUTH_THICK] = 4; }
      o[BROW] = cross; o[BROW_TILT] = -0.6f; o[LID] = 0.35f * cross; o[SQUEEZE] = wipe > 0.5f ? 1 : 0;
      if (tau > 5.3f && tau < 6.4f) o[HAPPY] = 1;
    } else if (m == HICCUP) {
      // three hiccups, holds its breath with puffed cheeks, lets go, and one more sneaks out
      float w = 0;
      for (int i = 0; i < 4; i++) { float x = tau - HIC[i]; if (x >= 0 && x < 0.5f) w = 1 - ease((x - 0.25f) / 0.2f); }
      float hd = win(tau, 3.4f, 5.4f, 0.15f), ex = win(tau, 5.4f, 5.9f, 0.08f);
      if (hd > 0.01f) {
        o[MOUTH_W] = lerpf(o[MOUTH_W], 6, hd); o[MOUTH_THICK] = 6; o[MOUTH_CURVE] = 3 * (1 - hd);
        o[LID] = 0.35f * hd; o[BROW] = hd; o[BROW_TILT] = -0.5f;
      }
      if (ex > 0.1f) { o[MOUTH_OPEN] = 1; o[MOUTH_W] = 9; o[MOUTH_CURVE] = 0; o[MOUTH_THICK] = 3; }
      if (tau > 5.9f && tau < 6.2f) o[HAPPY] = 1;
      if (tau > 6.6f) { o[LID] = 0.4f; o[MOUTH_W] = 16; o[MOUTH_CURVE] = 0; }
      if (w > 0) {
        o[EYE_R] = lerpf(11, 13, w); o[MOUTH_OPEN] = 0.8f * w; o[MOUTH_W] = lerpf(o[MOUTH_W], 7, w);
        o[BROW] = 0.6f * w; o[BROW_TILT] = 0; o[LID] = 0; o[HAPPY] = 0;
      }
    } else if (m == REV) {
      // bored, then three revs with the face shaking like an engine; caught, looks about, sheepish
      float e3 = win(tau, 3.7f, 5.0f, 0.3f);
      float e = 0.6f * win(tau, 1.5f, 2.2f, 0.25f) + 0.8f * win(tau, 2.6f, 3.3f, 0.25f) + e3;
      float on = win(tau, 1.0f, 5.2f, 0.2f), shy = win(tau, 5.5f, 6.5f, 0.15f);
      if (tau < 1.0f) { o[LID] = 0.45f; o[MOUTH_W] = 14; o[MOUTH_CURVE] = 0; }
      if (on > 0.01f) {
        o[BROW] = on; o[BROW_TILT] = -0.6f; o[LID] = 0.3f * on;
        o[MOUTH_W] = 12 + 8 * e; o[MOUTH_OPEN] = 0.2f + 0.8f * e; o[MOUTH_CURVE] = 0; o[MOUTH_THICK] = 3;
        o[LOOK_Y] = -2 * e; o[SPEED_FX] = 0.7f * e3;
      }
      if (tau >= 5.2f && tau < 5.5f) { o[EYE_R] = 13; o[MOUTH_W] = 8; o[MOUTH_CURVE] = 0; }
      if (shy > 0.01f) { o[BROW] = 0.6f * shy; o[BROW_TILT] = 0.5f; o[MOUTH_W] = 8; o[MOUTH_CURVE] = 0; o[MOUTH_THICK] = 3; }
      if (tau >= 6.5f) { o[MOUTH_W] = 20; o[MOUTH_CURVE] = 2; }
    } else if (m == PEEK) {
      // slides off the bottom, peeks up with just its eyes, ducks, pops up grinning
      if (tau < 0.6f) o[MOUTH_W] = 20;
      if (tau > 2.0f && tau < 4.2f) { o[EYE_R] = 12; o[BROW] = 0.5f; }
      if (tau >= 4.2f && tau < 6.3f) {
        o[HAPPY] = 1; o[MOUTH_W] = 24; o[MOUTH_GRIN] = 5; o[MOUTH_CURVE] = 0; o[MOUTH_THICK] = 4; o[EYE_R] = 12;
      }
    } else if (m == MIRROR) {
      // checks left, checks right, then a raised brow, a smirk and a wink
      float lk = win(tau, 0.3f, 2.5f, 0.15f), sm = win(tau, 3.0f, 5.3f, 0.25f);
      o[LOOK_X] = -5 * win(tau, 0.3f, 1.3f, 0.15f) + 5 * win(tau, 1.5f, 2.5f, 0.15f);
      if (lk > 0.01f) { o[MOUTH_W] = lerpf(32, 14, lk); o[MOUTH_CURVE] = 3 * (1 - lk); }
      if (sm > 0.01f) {
        o[LID] = 0.3f * sm; o[MOUTH_X] = 5 * sm; o[MOUTH_W] = lerpf(32, 18, sm); o[BROW] = 0.9f * sm; o[LOOK_Y] = -1 * sm;
      }
    } else if (m == DREAM) {
      // leans aside, gazes up at a thought bubble with a little turbo in it; it pops
      float d = tau < 6.8f ? ease((tau - 0.6f) / 1.0f) : 0;
      o[LOOK_X] = -18 * d; o[LOOK_Y] = 5 * d; o[EYE_SPREAD] = lerpf(41, 36, d); o[LID] = 0.25f * d;
      if (d > 0.01f) { o[MOUTH_W] = 20; o[MOUTH_CURVE] = 2.5f; o[MOUTH_THICK] = 4; }
      if (tau > 4.5f && tau < 6.2f) { o[HAPPY] = 1; o[LID] = 0; }
      if (tau >= 6.8f && tau < 7.3f) { o[EYE_R] = 13; o[MOUTH_W] = 8; o[MOUTH_OPEN] = 0.7f; o[MOUTH_CURVE] = 0; }
    }
  }
  static void actFx(Mood m, float tau, Fx& f) {
    f.ox = 0; f.oy = 0; f.cross = 0; f.winkR = 0; f.browR = 0;
    if (m == SNEEZE) {
      float x = tau - 2.9f;
      if (x >= 0 && x < 1.2f) { f.ox = 3 * sinf(x * 45) * expf(-6 * x); f.oy = 2 * cosf(x * 38) * expf(-6 * x); }
      f.cross = 3 * fmaxf(win(tau, 0.5f, 1.0f, 0.15f), fmaxf(win(tau, 1.4f, 1.95f, 0.15f), win(tau, 2.3f, 2.9f, 0.15f)));
    } else if (m == FLY) {
      f.cross = 7 * win(tau, 5.5f, 7.2f, 0.3f);
    } else if (m == HICCUP) {
      for (int i = 0; i < 4; i++) { float x = tau - HIC[i]; if (x >= 0 && x < 0.22f) f.oy = -5 * sinf(3.14159f * x / 0.22f); }
      f.ox = sinf(tau * 55) * 1.5f * clampf((tau - 3.4f) / 2, 0, 1) * win(tau, 3.4f, 5.4f, 0.15f);
    } else if (m == REV) {
      float e = 0.6f * win(tau, 1.5f, 2.2f, 0.25f) + 0.8f * win(tau, 2.6f, 3.3f, 0.25f) + win(tau, 3.7f, 5.0f, 0.3f);
      float idle = 0.3f * win(tau, 1.0f, 5.1f, 0.2f);
      f.ox = sinf(tau * 70) * (1.3f * e + idle); f.oy = cosf(tau * 83) * (0.9f * e + idle);
    } else if (m == PEEK) {
      float y = 62 * ease((tau - 0.5f) / 0.8f);
      if (tau >= 2.3f) y = lerpf(62, 31, ease((tau - 2.3f) / 0.6f));
      if (tau >= 3.7f) y = lerpf(31, 62, ease((tau - 3.7f) / 0.2f));
      if (tau >= 4.7f) y = lerpf(62, 0, ease((tau - 4.7f) / 0.3f));
      if (tau >= 5.0f) y = -4 * sinf((tau - 5.0f) * 16) * expf(-5 * (tau - 5.0f));
      f.oy = y;
    } else if (m == MIRROR) {
      f.winkR = win(tau, 3.9f, 4.35f, 0.08f); f.browR = 3 * win(tau, 3.0f, 5.3f, 0.25f);
    }
  }
  void actGaze(Mood m, float tau) {
    float gx = 0, gy = 0;
    if (m == FLY) {
      float q[2]; flyPos(tau, q);
      if (tau < 5.6f || (tau > 7.1f && tau < 7.9f)) { gx = clampf((q[0] - 64) / 5, -11, 11); gy = clampf((q[1] - 30) / 4, -4.5f, 4.5f); }
      else if (tau < 7.1f) gy = 3;
    } else if (m == WIPERS) {
      if (tau > 0.4f && tau < 1.6f) { int i = tau < 0.7f ? 0 : tau < 1.1f ? 1 : 2; gx = (DROPX[i] - 64) / 6; gy = -4; }
    } else if (m == REV) {
      if (tau < 1.0f) gx = 6 * sinf(tau * 2);
      else if (tau >= 5.5f && tau < 6.0f) gx = -9;
      else if (tau >= 6.0f && tau < 6.5f) gx = 9;
    } else if (m == PEEK) {
      if (tau < 0.6f) gy = 4;
      else if (tau >= 2.9f && tau < 3.3f) gx = -8;
      else if (tau >= 3.3f && tau < 3.7f) gx = 8;
    } else if (m == MIRROR) {
      if (tau >= 0.3f && tau < 1.3f) gx = -10;
      else if (tau >= 1.5f && tau < 2.5f) gx = 10;
    } else if (m == DREAM) {
      if (tau > 0.4f && tau < 6.8f) { gx = 8; gy = -4; }
    } else if (m == HICCUP) {
      if (tau > 6.6f) gx = 7;
    }
    gtx_ = gx; gty_ = gy;
  }
  void sblink(float tau, float t0) {
    float x = tau - t0;
    if (x >= 0 && x < 0.25f) { blinkStart_ = t_ - x; blinkSlow_ = 1; }
  }
  void actBlink(Mood m, float tau) {
    if (m == SNEEZE) { sblink(tau, 3.6f); sblink(tau, 4.0f); }
    else if (m == FLY) { sblink(tau, 2.0f); sblink(tau, 4.2f); sblink(tau, 8.2f); }
    else if (m == WIPERS) sblink(tau, 5.15f);
    else if (m == HICCUP) { for (int i = 0; i < 4; i++) sblink(tau, HIC[i] + 0.45f); }
    else if (m == REV) { sblink(tau, 0.5f); sblink(tau, 6.7f); }
    else if (m == PEEK) sblink(tau, 6.4f);
    else if (m == MIRROR) sblink(tau, 5.45f);
    else if (m == DREAM) sblink(tau, 7.3f);
  }
  void actOverlay(Mood m, float tau, float cx, float cy, float mx, float my) {
    if (m == SNEEZE) {  // the spray
      float x = (tau - 2.9f) / 0.35f;
      if (x >= 0 && x < 1) for (int i = 0; i < 6; i++) {
        float a = (i % 3 - 1) * 0.35f, sd = i < 3 ? 1 : -1, r0 = 12 + 26 * x;
        capsule(mx + sd * cosf(a) * r0, my + sinf(a) * r0, mx + sd * cosf(a) * (r0 + 5), my + sinf(a) * (r0 + 5), 0.7f);
      }
    } else if (m == FLY) {
      if (tau < 7.9f) {
        float q[2]; flyPos(tau, q);
        float wy = (tau > 5.8f && tau < 7.1f) ? 0.5f : (sinf(t_ * 60) > 0 ? 2.5f : 0.5f);
        capsule(q[0] - 0.8f, q[1], q[0] + 0.8f, q[1], 1.3f);
        capsule(q[0] - 1, q[1] - 1, q[0] - 3.5f, q[1] - 1 - wy, 0.6f);
        capsule(q[0] + 1, q[1] - 1, q[0] + 3.5f, q[1] - 1 - wy, 0.6f);
      }
      float u = (tau - 7.0f) / 0.35f;  // the puff that blows it off
      if (u > 0 && u < 1) for (int i = -1; i <= 1; i++)
        capsule(mx + i * (2 + 3 * u), my - 5 - 8 * u, mx + i * (3 + 4 * u), my - 9 - 8 * u, 0.7f);
    } else if (m == WIPERS) {
      float s1 = ease((tau - 3.0f) / 0.9f), s2 = ease((tau - 4.2f) / 0.9f);
      float phi = 3.0543f - 2.9671f * (s1 - s2), swept = 3.0543f - 2.9671f * s1;
      for (int i = 0; i < 6; i++) {
        float ta = 0.3f + 0.4f * i;
        if (tau < ta || atan2f(80 - DROPY[i], DROPX[i] - 64) > swept) continue;
        float y = DROPY[i] - 14 * (1 - clampf((tau - ta) / 0.15f, 0, 1));
        capsule(DROPX[i], y - 3.5f, DROPX[i], y, 0.7f);
        capsule(DROPX[i], y, DROPX[i], y, 1.9f);
      }
      if (s1 > 0.001f && s2 < 0.999f)
        capsule(64 + 22 * cosf(phi), 80 - 22 * sinf(phi), 64 + 84 * cosf(phi), 80 - 84 * sinf(phi), 1.6f);
    } else if (m == HICCUP) {
      if (win(tau, 3.4f, 5.4f, 0.15f) > 0.5f) for (int s = -1; s <= 1; s += 2) {  // puffed cheeks
        capsule(mx + s * 10, my - 4, mx + s * 13, my, 0.8f);
        capsule(mx + s * 13, my, mx + s * 10, my + 4, 0.8f);
      }
      for (int i = 0; i < 4; i++) {
        float x = tau - HIC[i];
        if (x >= 0 && x < 0.3f) { capsule(cx + 57, cy + 7, cx + 60, cy + 3, 0.7f); capsule(cx + 60, cy + 11, cx + 63, cy + 9, 0.7f); }
      }
      float u = (tau - 5.4f) / 0.5f;
      if (u > 0 && u < 1) for (int i = -1; i <= 1; i++)
        capsule(mx + 9 + u * 16, my + i * (3 + u * 3), mx + 14 + u * 16 - (i ? 1 : 0), my + i * (4 + u * 4), 0.8f);
    } else if (m == MIRROR) {  // a glint off the wink
      float u = (tau - 4.0f) / 0.7f;
      if (u > 0 && u < 1) {
        float s = 4.5f * sinf(3.14159f * u), sx = cx + cur_[EYE_SPREAD] + 14, sy = cur_[EYE_Y] + cy - 13;
        capsule(sx - s, sy, sx + s, sy, 0.6f); capsule(sx, sy - s, sx, sy + s, 0.6f);
      }
    } else if (m == DREAM) {
      if (tau > 1.2f && tau < 6.8f) capsule(103, 33, 103, 33, 1.3f);
      if (tau > 1.5f && tau < 6.8f) capsule(107, 27, 107, 27, 1.9f);
      float R = 11 * ease((tau - 1.8f) / 0.5f);
      if (tau < 6.8f && R > 1.5f) {
        capsule(114, 12, 114, 12, R);
        fg_ = 1 - fg_;
        capsule(114, 12, 114, 12, R - 1.3f);
        fg_ = 1 - fg_;
        if (R > 8) {  // the little turbo it is dreaming about
          for (int i = 0; i < 3; i++) {
            float a = tau * 4 + i * 2.0944f;
            float x1 = 114 + 5 * cosf(a - 0.35f), y1 = 12 + 5 * sinf(a - 0.35f);
            capsule(114 + 2 * cosf(a), 12 + 2 * sinf(a), x1, y1, 1.0f);
            capsule(x1, y1, 114 + 7.5f * cosf(a - 0.85f), 12 + 7.5f * sinf(a - 0.85f), 0.8f);
          }
          capsule(114, 12, 114, 12, 1.7f);
        }
      }
      float u = (tau - 6.8f) / 0.25f;  // pop
      if (u >= 0 && u < 1) for (int i = 0; i < 6; i++) {
        float a = i * 1.0472f + 0.5f, r0 = 8 + 8 * u;
        capsule(114 + r0 * cosf(a), 12 + r0 * sinf(a), 114 + (r0 + 3) * cosf(a), 12 + (r0 + 3) * sinf(a), 0.7f);
      }
    }
  }

  // Turbo act timeline (seconds since it started). Pure functions of time, so
  // it plays the same at any frame rate.
  //   0.0 - 1.0   "phew"
  //   1.0 - 3.2   flushed: the eyes and mouth are dragged round the centre,
  //               faster and faster, smearing into three swept blades
  //   3.2 - 5.6   the wheel spools up and back down
  //   5.6 - 7.4   it slows to a stop while the blades unwind into the face
  //   7.4 - 9.4   queasy
  //   9.4 - 11.4  worn out, with a sigh
  //   morph: 0 = face, 1 = wheel.  theta: how far round it has turned.
  static float ease(float u) { u = clampf(u, 0, 1); return u * u * u * (u * (u * 6 - 15) + 10); }
  static float turboTau(float tau) { return tau >= 12.4f ? tau - 12.4f * floorf(tau / 12.4f) : tau; }
  static void turboState(float tau, float& morph, float& theta) {
    morph = 0; theta = 0;
    if (tau < 1.0f || tau >= 7.4f) return;
    if (tau < 3.2f) {
      float x = tau - 1.0f;
      theta = 1.5909f * x * x; morph = ease(x / 2.2f);          // 0 -> 7 rad/s
    } else if (tau < 5.6f) {
      float x = tau - 3.2f;
      theta = 7.7f + 7 * x + 3.4496f * (1 - cosf(1.309f * x)); morph = 1;  // peaks at 11.5 rad/s
    } else {
      float y = 7.4f - tau;                                     // 7 -> 0 rad/s, lands upright after 6 turns
      theta = 37.6991f - 1.9444f * y * y; morph = ease(y / 1.8f);
    }
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

  // Rounded box of teeth: outline filled, then gaps cut back out in the
  // background colour. `bend` lifts the middle (the wobbly grimace).
  void teethMouth(float cx, float cy, float w, float h, float bend) {
    float hw = w / 2;
    int x0 = (int)floorf(cx - hw), x1 = (int)ceilf(cx + hw);
    uint8_t bg = fg_ ? 0 : 1;
    for (int x = x0; x <= x1; x++) {
      float t = (x + 0.5f - cx) / hw;
      if (fabsf(t) >= 1) continue;
      float u = 1 - t * t;
      float mid = cy - bend * u + sinf(t * 7.5f + t_ * 23) * 0.6f;
      float ht = h / 2 * (fabsf(t) > 0.85f ? 0.7f : 1);
      vspan(x, mid - ht, mid + ht);
      if (x <= x0 + 1 || x >= x1 - 1 || x < 0 || x > 127) continue;
      // tooth gaps every 4 px, and the line where the teeth meet
      if (((x - x0) & 3) == 0) {
        int a = (int)ceilf(mid - ht + 1.5f - 0.5f), b = (int)floorf(mid + ht - 1.5f - 0.5f);
        if (a < 0) a = 0;
        if (b > 63) b = 63;
        if (b >= a) d_.drawFastVLine(x, a, b - a + 1, bg);
      }
      int my = (int)floorf(mid);
      if (my > 0 && my < 63 && fabsf(t) < 0.9f) d_.drawFastHLine(x, my, 1, bg);
    }
  }

  void zee(float x, float y, float s) {
    capsule(x, y, x + s, y, 0.9f);
    capsule(x + s, y, x, y + s, 0.9f);
    capsule(x, y + s, x + s, y + s, 0.9f);
  }

  // Eighth note; `flag` alternates the tail so they don't all look alike.
  void note(float x, float y, int flag) {
    capsule(x - 0.6f, y + 0.4f, x + 0.8f, y - 0.3f, 1.7f);          // head
    capsule(x + 2.1f, y - 0.3f, x + 2.1f, y - 7.5f, 0.6f);          // stem
    if (flag) capsule(x + 2.1f, y - 7.5f, x + 5.0f, y - 5.0f, 0.7f);  // tail
    else capsule(x + 2.1f, y - 7.5f, x + 4.5f, y - 7.0f, 0.9f);
  }

  // The flush. The right eye, the mouth and the left eye are each a 6-point
  // spine that starts collapsed on the feature (morph 0) and is dragged round
  // the centre, the far end trailing behind, until it lies along one long
  // swept blade of a turbo wheel (morph 1). The swirl starts as a flat oval,
  // like water circling a bowl, so nothing leaves the screen, and opens into
  // a round wheel as the features are pulled in. Only the three blades and a
  // small hub are drawn.
  void turbo(float cx, float cy, float morph, float theta) {
    const float* p = cur_;
    static const float BETA[3] = { -0.5236f, 1.5708f, 3.6652f };   // blade angles, 120 deg apart
    static const float BR[6] = { 5, 9.6f, 14.2f, 18.8f, 23.4f, 28 };          // spine radii
    static const float BSW[6] = { 0, 0.167f, 0.41f, 0.695f, 1.01f, 1.35f };   // sweep-back at each
    static const float BT[6] = { 3.0f, 2.7f, 2.4f, 2.1f, 1.8f, 1.5f };    // half-thickness at each
    const float SY0 = 0.45f;
    float q = 1 - (1 - morph) * (1 - morph);
    float sy = lerpf(SY0, 1, q), smear = 0.6f * sinf(3.14159f * morph);
    float ex = p[EYE_SPREAD], ey = (p[EYE_Y] - 32) / SY0, my = (p[MOUTH_Y] - 32) / SY0;
    float fr[3], fa[3], fs[3];   // each feature: distance and angle round the oval, size
    fr[0] = fr[2] = sqrtf(ex * ex + ey * ey);
    fa[0] = atan2f(ey, ex); fa[2] = 3.14159f - fa[0];
    fr[1] = my; fa[1] = 1.5708f;
    fs[0] = fs[2] = p[EYE_R]; fs[1] = 5.5f;
    for (int i = 0; i < 3; i++) {
      float lx = 0, ly = 0, lr = 0;
      for (int k = 0; k < 6; k++) {
        float rr = lerpf(fr[i], BR[k], q);
        float a = lerpf(fa[i], BETA[i] - BSW[k], morph) + theta - smear * k * 0.2f;
        float rad = lerpf(fs[i], BT[k], q);
        float x = cx + rr * cosf(a), y = cy + sy * rr * sinf(a);
        if (k) capsule(lx, ly, x, y, (lr + rad) * 0.5f);
        lx = x; ly = y; lr = rad;
      }
    }
    if (morph > 0.15f) capsule(cx, cy, cx, cy, 4.8f * morph);  // hub
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
