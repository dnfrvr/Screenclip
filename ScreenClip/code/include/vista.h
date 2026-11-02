#pragma once

#include <Arduino.h>

#include "reglages.h"
#include "fonts_data.h"
#include "logo_image.h"
#include "texte.h"

namespace vista {
constexpr uint32_t kBootMs = 3500;

// DA « système 2001 » : un fond rose pâle uni, une barre rose, du pixel net, du vert pour le reste.
constexpr uint16_t kInk = 0x0A47;
constexpr uint16_t kPaper = 0xFFFF;
constexpr uint16_t kPink = 0xF952;
constexpr uint16_t kDeepPink = 0xB08C;
constexpr uint16_t kGround = 0xFF7E;
constexpr uint16_t kGreen = 0x1E0F;
constexpr uint16_t kMoss = 0x2BCC;
constexpr int kBarH = 54;
constexpr int kLeft = 28;

inline uint16_t *cache = nullptr;
inline bool cached = false;

inline int lerp(int a, int b, float t) { return a + (int)((b - a) * t); }
inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

inline void sparkle(Canvas &c, int x, int y, int scale, uint16_t color) {
  static const char *const kShape[] = {"....#....", "....#....", "...###...", "..#####..", "#########",
                                       "..#####..", "...###...", "....#....", "....#...."};
  for (int r = 0; r < 9; ++r) {
    for (int col = 0; col < 9; ++col) {
      if (kShape[r][col] == '#') c.fillRect(x + col * scale, y + r * scale, scale, scale, color);
    }
  }
}

inline void background(Canvas &c) {
  if (!cache) cache = static_cast<uint16_t *>(heap_caps_malloc(c.w * c.h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!cache) { c.fill(kGround); return; }
  if (!cached) {
    Canvas target = {cache, c.w, c.h};
    target.fill(kGround);
    target.fillRect(0, 0, c.w, kBarH, kPink);
    sparkle(target, c.w - 50, 14, 3, kPaper);
    cached = true;
  }
  memcpy(c.pixels, cache, c.w * c.h * 2);
}

inline void window(Canvas &c, const char *title) {
  background(c);
  drawText(c, kFontUi, kLeft, 37, kPaper, title);
}

// Barre de progression en blocs qui défilent.
inline void blocks(Canvas &c, int x, int y, int w, uint32_t ms) {
  constexpr int kH = 22, kBlock = 14, kGap = 4, kLit = 5;
  c.fillRect(x, y, w, kH, kInk);
  c.fillRect(x + 2, y + 2, w - 4, kH - 4, kPaper);
  const int slots = (w - 8) / (kBlock + kGap);
  const int head = (ms / 120) % (slots + kLit);
  for (int i = head - kLit; i < head; ++i) {
    if (i >= 0 && i < slots) c.fillRect(x + 5 + i * (kBlock + kGap), y + 5, kBlock, kH - 10, kGreen);
  }
}

inline void roundRect(Canvas &c, int x, int y, int w, int h, float radius, const uint8_t top[3],
                      const uint8_t bottom[3], uint8_t opacity = 255) {
  const float hx = w / 2.0f, hy = h / 2.0f, cx = x + hx, cy = y + hy;
  for (int py = y - 1; py <= y + h; ++py) {
    if (py < 0 || py >= c.h) continue;
    const float t = clamp01((py - y) / (float)h);
    const uint16_t color = rgb565(lerp(top[0], bottom[0], t), lerp(top[1], bottom[1], t), lerp(top[2], bottom[2], t));
    for (int px = x - 1; px <= x + w; ++px) {
      if (px < 0 || px >= c.w) continue;
      const float qx = fabsf(px + 0.5f - cx) - (hx - radius), qy = fabsf(py + 0.5f - cy) - (hy - radius);
      const float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
      const float dist = sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0) - radius;
      const float a = clamp01(0.5f - dist);
      if (a <= 0) continue;
      uint16_t &p = c.pixels[py * c.w + px];
      p = blend565(p, color, (uint8_t)(a * opacity));
    }
  }
}

inline void drawBoot(Canvas &c, uint32_t ms) {
  c.fill(kGround);
  const uint8_t logoOpacity = ms < 200 ? 0 : (ms < 900 ? (ms - 200) * 255 / 700 : 255);
  const int logoX = (c.w - kLogoW) / 2, logoY = 72;
  drawImage(c, logoX, logoY, kLogoW, kLogoH, kLogoColor, kLogoAlpha, logoOpacity);
  sparkle(c, c.w - 60, 20, 3, kGreen);
  if (ms > 600) blocks(c, (c.w - 300) / 2, 262, 300, ms);
}

inline void drawMessage(Canvas &c, const char *title, const char *text, int32_t spinnerMs) {
  window(c, "Screenclip");
  drawText(c, kFontUiBig, kLeft, 150, kInk, title);
  if (text) drawText(c, kFontUi, kLeft, 202, kMoss, text);
  if (spinnerMs >= 0) blocks(c, kLeft, 244, 420, spinnerMs);
}

inline void drawHelp(Canvas &c) {
  static const char *const kRows[][2] = {
      {"Appui court", "écran suivant"},
      {"Deux appuis", "écran précédent"},
      {"Appui long", "ouvre les réglages"},
      {"Dans les réglages", "court : ligne suivante, long : valider"},
  };
  window(c, "Mode d'emploi");
  for (int i = 0; i < 4; ++i) {
    const int y = 110 + i * 52;
    drawText(c, kFontUi, kLeft, y, kDeepPink, kRows[i][0]);
    drawText(c, kFontUi, 300, y, kMoss, kRows[i][1]);
  }
}

// Colonne de droite de la page Téléphone : le QR code occupe la gauche.
inline void drawPhone(Canvas &c, const char *title, const char *const *lines, int count, int primary) {
  window(c, title);
  c.fillRect(14, 62, 252, 252, kInk);
  for (int i = 0; i < count; ++i) drawText(c, kFontUi, 292, 112 + i * 44, i < primary ? kInk : kMoss, lines[i]);
}

constexpr int kHintW = 224, kHintH = 84;

inline void drawRotateHint(Canvas &c) {
  c.fill(kInk);
  drawText(c, kFontUi, 14, 34, kPaper, "Appui : tourner");
  drawText(c, kFontUi, 14, 70, kPaper, "Long : valider");
}

constexpr int kSaverW = 36, kSaverH = 36;

inline void drawSaver(Canvas &c, int x, int y) {
  c.fill(0x0000);
  sparkle(c, x, y, 4, kPink);
}

struct SettingRow {
  const char *label;
  const char *value;
};

inline void drawSettings(Canvas &c, const SettingRow *rows, int count, int selected) {
  window(c, "Réglages");
  for (int i = 0; i < count; ++i) {
    const int y = 86 + i * 36;
    const bool on = i == selected;
    if (on) c.fillRect(0, y - 26, c.w, 36, kInk);
    drawText(c, kFontUi, kLeft, y, on ? kPaper : kInk, rows[i].label);
    drawText(c, kFontUi, 370, y, on ? kPaper : kMoss, rows[i].value);
  }
  drawText(c, kFontUi, kLeft, 306, kMoss, "Appui court : ligne suivante   Appui long : valider");
}
}
