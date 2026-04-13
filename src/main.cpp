#define PxMATRIX_double_buffer true

#include <Arduino.h>
#include <PxMatrix.h>
#include <math.h>

#define P_A    19
#define P_B    23
#define P_LAT  22
#define P_OE   16

#define W  32
#define H  16

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t display_draw_time = 1;

PxMATRIX display(W, H, P_LAT, P_OE, P_A, P_B);

void IRAM_ATTR display_updater() {
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}

void display_update_enable(bool is_enable) {
  if (is_enable) {
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &display_updater, true);
    timerAlarmWrite(timer, 2000, true);
    timerAlarmEnable(timer);
  } else {
    timerDetachInterrupt(timer);
    timerAlarmDisable(timer);
  }
}

uint16_t gGreen, gBlack;

// === Qoima logo ===
static bool logoBuf[14][14];

void computeLogo() {
  float cx = 5.5f, cy = 5.5f, R = 5.9f, r = 2.8f;
  for (int y = 0; y < 14; y++) {
    for (int x = 0; x < 14; x++) {
      float dx = x - cx, dy = y - cy;
      float dist = sqrtf(dx * dx + dy * dy);
      bool px = false;
      if (dist >= r && dist <= R) {
        px = true;
        if (dx > 0 && dy > 0 && atan2f(dy, dx) > 0.25f && atan2f(dy, dx) < 1.25f) px = false;
      }
      if (x >= 4 && x <= 6 && y >= 4 && y <= 6) px = true;
      if (x >= 8 && y >= 8 && x <= 12 && y <= 12 && fabsf((float)(x-8) - (float)(y-8)) < 1.2f) px = true;
      if (x >= 11 && x <= 12 && y >= 11 && y <= 12) px = true;
      logoBuf[y][x] = px;
    }
  }
}

void drawLogo(int ox, int oy, uint16_t color) {
  for (int y = 0; y < 14; y++)
    for (int x = 0; x < 14; x++)
      if (logoBuf[y][x]) {
        int px = ox + x, py = oy + y;
        if (px >= 0 && px < W && py >= 0 && py < H)
          display.drawPixel(px, py, color);
      }
}

// === HSV to RGB565 ===
uint16_t hsv(uint8_t h, uint8_t s, uint8_t v) {
  uint8_t r, g, b;
  uint8_t region = h / 43;
  uint8_t rem = (h - region * 43) * 6;
  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * rem) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8;
  switch (region) {
    case 0:  r=v; g=t; b=p; break;
    case 1:  r=q; g=v; b=p; break;
    case 2:  r=p; g=v; b=t; break;
    case 3:  r=p; g=q; b=v; break;
    case 4:  r=t; g=p; b=v; break;
    default: r=v; g=p; b=q; break;
  }
  return display.color565(r, g, b);
}

// =====================================================
// EFFECT 1: Qoima intro (scroll text + logo slide in)
// =====================================================
void effectQoimaIntro() {
  display.setTextWrap(false);
  display.setTextSize(1);
  display.setTextColor(gGreen);

  for (int x = W; x > -35; x--) {
    display.clearDisplay();
    display.setCursor(x, 4);
    display.print("Qoima");
    display.showBuffer();
    delay(28);
  }
  delay(150);

  int tX = (W - 14) / 2, tY = 1;
  for (int x = W; x >= tX; x--) {
    display.clearDisplay();
    drawLogo(x, tY, gGreen);
    display.showBuffer();
    delay(12);
  }
  delay(2000);
}

// =====================================================
// EFFECT 2: Matrix rain
// =====================================================
#define RAIN_COLS W
static float rainY[RAIN_COLS];
static float rainSpeed[RAIN_COLS];
static uint8_t rainLen[RAIN_COLS];

void initRain() {
  for (int i = 0; i < RAIN_COLS; i++) {
    rainY[i] = random(-H, 0);
    rainSpeed[i] = 0.3f + random(0, 100) / 100.0f * 0.7f;
    rainLen[i] = 3 + random(0, 6);
  }
}

void effectMatrixRain(int durationMs) {
  initRain();
  unsigned long start = millis();

  while (millis() - start < (unsigned long)durationMs) {
    display.clearDisplay();

    for (int col = 0; col < RAIN_COLS; col++) {
      int head = (int)rainY[col];
      for (int t = 0; t < rainLen[col]; t++) {
        int py = head - t;
        if (py >= 0 && py < H) {
          uint8_t bright = 255 - (t * 255 / rainLen[col]);
          display.drawPixel(col, py, display.color565(0, bright, 0));
        }
      }
      rainY[col] += rainSpeed[col];
      if ((int)rainY[col] - rainLen[col] > H) {
        rainY[col] = random(-6, 0);
        rainSpeed[col] = 0.3f + random(0, 100) / 100.0f * 0.7f;
        rainLen[col] = 3 + random(0, 6);
      }
    }
    display.showBuffer();
    delay(30);
  }
}

// =====================================================
// EFFECT 3: Rainbow plasma
// =====================================================
void effectPlasma(int durationMs) {
  unsigned long start = millis();
  float t = 0;

  while (millis() - start < (unsigned long)durationMs) {
    for (int y = 0; y < H; y++) {
      for (int x = 0; x < W; x++) {
        float v = sinf(x * 0.3f + t)
                + sinf(y * 0.4f + t * 0.7f)
                + sinf((x + y) * 0.2f + t * 0.5f)
                + sinf(sqrtf(x * x + y * y) * 0.3f - t * 0.8f);
        uint8_t hue = (uint8_t)((v + 4.0f) * 32.0f);
        display.drawPixel(x, y, hsv(hue, 255, 180));
      }
    }
    display.showBuffer();
    t += 0.12f;
    delay(20);
  }
}

// =====================================================
// EFFECT 4: Fireworks
// =====================================================
#define MAX_SPARKS 40

struct Spark {
  float x, y, vx, vy;
  uint8_t hue;
  int life;
};

void effectFireworks(int durationMs) {
  Spark sparks[MAX_SPARKS];
  int nSparks = 0;
  unsigned long start = millis();
  unsigned long nextLaunch = 0;

  while (millis() - start < (unsigned long)durationMs) {
    display.clearDisplay();

    // Launch new firework
    if (millis() > nextLaunch) {
      float cx = 6 + random(0, W - 12);
      float cy = 2 + random(0, 6);
      uint8_t hue = random(0, 256);
      int count = 12 + random(0, 10);

      for (int i = 0; i < count && nSparks < MAX_SPARKS; i++) {
        float angle = (float)i / count * 6.2832f;
        float speed = 0.5f + random(0, 100) / 100.0f * 1.5f;
        sparks[nSparks] = {cx, cy, cosf(angle) * speed, sinf(angle) * speed, hue, 20 + random(0, 15)};
        nSparks++;
      }
      nextLaunch = millis() + 400 + random(0, 600);
    }

    // Update & draw sparks
    int alive = 0;
    for (int i = 0; i < nSparks; i++) {
      Spark &s = sparks[i];
      s.x += s.vx;
      s.y += s.vy;
      s.vy += 0.05f; // gravity
      s.life--;

      if (s.life > 0 && s.x >= 0 && s.x < W && s.y >= 0 && s.y < H) {
        uint8_t bright = (s.life > 10) ? 255 : s.life * 25;
        display.drawPixel((int)s.x, (int)s.y, hsv(s.hue, 200, bright));
        sparks[alive++] = s;
      }
    }
    nSparks = alive;

    display.showBuffer();
    delay(25);
  }
}

// =====================================================
// EFFECT 5: DVD bouncing logo
// =====================================================
void effectBouncingLogo(int durationMs) {
  float x = 2, y = 1;
  float vx = 0.6f, vy = 0.4f;
  uint8_t hue = 0;
  unsigned long start = millis();

  while (millis() - start < (unsigned long)durationMs) {
    display.clearDisplay();

    drawLogo((int)x, (int)y, hsv(hue, 255, 220));
    display.showBuffer();

    x += vx;
    y += vy;

    if (x <= 0 || x + 14 >= W) { vx = -vx; hue += 40; }
    if (y <= 0 || y + 14 >= H) { vy = -vy; hue += 60; }

    delay(20);
  }
}

// =====================================================
// EFFECT 6: Fire effect
// =====================================================
static uint8_t fireGrid[H][W];

void effectFire(int durationMs) {
  memset(fireGrid, 0, sizeof(fireGrid));
  unsigned long start = millis();

  while (millis() - start < (unsigned long)durationMs) {
    // Heat bottom row
    for (int x = 0; x < W; x++)
      fireGrid[H - 1][x] = 150 + random(0, 106);

    // Propagate upward with cooling
    for (int y = 0; y < H - 1; y++) {
      for (int x = 0; x < W; x++) {
        int x0 = (x - 1 + W) % W;
        int x1 = (x + 1) % W;
        int avg = (fireGrid[y + 1][x0] + fireGrid[y + 1][x] * 2 + fireGrid[y + 1][x1]) / 4;
        int cool = random(0, 8);
        fireGrid[y][x] = (avg > cool) ? avg - cool : 0;
      }
    }

    // Render
    for (int y = 0; y < H; y++) {
      for (int x = 0; x < W; x++) {
        uint8_t v = fireGrid[y][x];
        uint8_t r, g, b;
        if (v < 85) {
          r = v * 3; g = 0; b = 0;
        } else if (v < 170) {
          r = 255; g = (v - 85) * 3; b = 0;
        } else {
          r = 255; g = 255; b = (v - 170) * 3;
        }
        display.drawPixel(x, y, display.color565(r, g, b));
      }
    }
    display.showBuffer();
    delay(35);
  }
}

// =====================================================
// EFFECT 7: Starfield (3D warp)
// =====================================================
#define NUM_STARS 50

struct Star {
  float x, y, z;
};

void effectStarfield(int durationMs) {
  Star stars[NUM_STARS];
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i] = {(float)random(-500, 500) / 10.0f,
                (float)random(-500, 500) / 10.0f,
                (float)random(1, 100) / 10.0f};
  }

  unsigned long start = millis();
  float speed = 0.15f;

  while (millis() - start < (unsigned long)durationMs) {
    display.clearDisplay();

    for (int i = 0; i < NUM_STARS; i++) {
      stars[i].z -= speed;
      if (stars[i].z <= 0.1f) {
        stars[i].x = (float)random(-500, 500) / 10.0f;
        stars[i].y = (float)random(-500, 500) / 10.0f;
        stars[i].z = 10.0f;
      }

      int sx = (int)(stars[i].x / stars[i].z * 8.0f + W / 2);
      int sy = (int)(stars[i].y / stars[i].z * 4.0f + H / 2);

      if (sx >= 0 && sx < W && sy >= 0 && sy < H) {
        uint8_t bright = (uint8_t)(255.0f * (1.0f - stars[i].z / 10.0f));
        display.drawPixel(sx, sy, display.color565(bright, bright, bright));
      }
    }
    display.showBuffer();
    speed += 0.0003f; // accelerate
    delay(20);
  }
}

// =====================================================
// EFFECT 8: Snake (auto-play)
// =====================================================
#define SNAKE_MAX 60

void effectSnake(int durationMs) {
  int sx[SNAKE_MAX], sy[SNAKE_MAX];
  int sLen = 3, dir = 0; // 0=right,1=down,2=left,3=up
  int dx[] = {1, 0, -1, 0};
  int dy[] = {0, 1, 0, -1};
  int foodX, foodY;
  uint8_t hue = 0;

  sx[0] = W / 2; sy[0] = H / 2;
  sx[1] = sx[0] - 1; sy[1] = sy[0];
  sx[2] = sx[0] - 2; sy[2] = sy[0];

  foodX = random(1, W - 1);
  foodY = random(1, H - 1);

  unsigned long start = millis();

  while (millis() - start < (unsigned long)durationMs) {
    // AI: turn toward food
    int headX = sx[0], headY = sy[0];
    int diffX = foodX - headX, diffY = foodY - headY;

    // Prefer moving toward food, avoid self
    int bestDir = dir;
    int bestDist = 9999;

    for (int d = 0; d < 4; d++) {
      int nx = headX + dx[d], ny = headY + dy[d];
      if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;

      // Don't hit self
      bool hit = false;
      for (int j = 0; j < sLen - 1; j++)
        if (sx[j] == nx && sy[j] == ny) { hit = true; break; }
      if (hit) continue;

      int dist = abs(foodX - nx) + abs(foodY - ny);
      if (dist < bestDist) { bestDist = dist; bestDir = d; }
    }
    dir = bestDir;

    // Move
    for (int i = sLen - 1; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
    sx[0] += dx[dir];
    sy[0] += dy[dir];

    // Wrap
    sx[0] = (sx[0] + W) % W;
    sy[0] = (sy[0] + H) % H;

    // Eat food
    if (sx[0] == foodX && sy[0] == foodY) {
      if (sLen < SNAKE_MAX) sLen++;
      foodX = random(1, W - 1);
      foodY = random(1, H - 1);
      hue += 30;
    }

    // Draw
    display.clearDisplay();
    display.drawPixel(foodX, foodY, display.color565(255, 0, 0));
    for (int i = 0; i < sLen; i++) {
      uint8_t fade = 255 - (i * 200 / sLen);
      display.drawPixel(sx[i], sy[i], hsv(hue + i * 3, 255, fade));
    }
    display.showBuffer();
    delay(70);
  }
}

// =====================================================
// MAIN
// =====================================================
void setup() {
  Serial.begin(9600);
  display.begin(4);
  display.setFastUpdate(true);
  display.setScanPattern(ZAGZIG);

  gGreen = display.color565(40, 210, 50);
  gBlack = display.color565(0, 0, 0);

  display.clearDisplay();
  display_update_enable(true);
  computeLogo();
}

void loop() {
  effectQoimaIntro();       // Qoima text + logo
  effectMatrixRain(6000);   // Matrix rain
  effectPlasma(6000);       // Rainbow plasma
  effectFireworks(6000);    // Fireworks
  effectFire(6000);         // Realistic fire
  effectStarfield(6000);    // 3D star warp
  effectBouncingLogo(6000); // DVD bouncing Qoima logo
  effectSnake(8000);        // Self-playing snake
}
