#pragma once

#include <Arduino.h>

#include "reglages.h"
#include "fonts_data.h"
#include "logo_image.h"
#include "texte.h"

namespace vista {
constexpr uint32_t kBootMs = 3500;

// DA « système 2001 » : un fond lilas, une barre de titre, du pixel net, un seul rose.
constexpr uint16_t kInk = 0x10AB;
constexpr uint16_t kPaper = 0xFFFF;
constexpr uint16_t kPink = 0xF952;
constexpr uint16_t kPurple = 0x59FA;
constexpr int kBarH = 54;
constexpr int kLeft = 28;

inline uint16_t *cache = nullptr;
inline bool cached = false;

inline int lerp(int a, int b, float t) { return a + (int)((b - a) * t); }
inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

inline uint16_t dithered(float r, float g, float b, int x, int y) {
  static const int8_t kBayer[4][4] = {{-8, 0, -6, 2}, {4, -4, 6, -2}, {-5, 3, -7, 1}, {7, -1, 5, -3}};
  const float d = kBayer[y & 3][x & 3] / 16.0f;
  const int ri = (int)(r + d * 8 + 0.5f), gi = (int)(g + d * 4 + 0.5f), bi = (int)(b + d * 8 + 0.5f);
  return rgb565(ri < 0 ? 0 : (ri > 255 ? 255 : ri), gi < 0 ? 0 : (gi > 255 ? 255 : gi), bi < 0 ? 0 : (bi > 255 ? 255 : bi));
}

inline void sparkle(Canvas &c, int x, int y, int scale, uint16_t color) {
  static const char *const kShape[] = {"....#....", "....#....", "...###...", "..#####..", "#########",
                                       "..#####..", "...###...", "....#....", "....#...."};
  for (int r = 0; r < 9; ++r) {
    for (int col = 0; col < 9; ++col) {
      if (kShape[r][col] == '#') c.fillRect(x + col * scale, y + r * scale, scale, scale, color);
    }
  }
}

inline void ground(Canvas &c) {
  for (int y = 0; y < c.h; ++y) {
    const float t = (float)y / c.h;
    for (int x = 0; x < c.w; ++x) c.pixels[y * c.w + x] = dithered(lerp(236, 255, t), lerp(233, 228, t), lerp(255, 246, t), x, y);
  }
}

inline void background(Canvas &c) {
  if (!cache) cache = static_cast<uint16_t *>(heap_caps_malloc(c.w * c.h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!cache) { c.fill(0xEF5F); return; }
  if (!cached) {
    Canvas target = {cache, c.w, c.h};
    ground(target);
    for (int y = 0; y < kBarH; ++y) {
      for (int x = 0; x < c.w; ++x) {
        const float t = (float)x / c.w;
        cache[y * c.w + x] = dithered(lerp(44, 196, t), lerp(36, 60, t), lerp(214, 240, t), x, y);
      }
    }
    target.fillRect(0, kBarH, c.w, 2, kInk);
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
    if (i >= 0 && i < slots) c.fillRect(x + 5 + i * (kBlock + kGap), y + 5, kBlock, kH - 10, kPink);
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
  ground(c);
  const uint8_t logoOpacity = ms < 200 ? 0 : (ms < 900 ? (ms - 200) * 255 / 700 : 255);
  const int logoX = (c.w - kLogoW) / 2, logoY = 58 - kLogoPad;
  drawImage(c, logoX, logoY, kLogoW, kLogoH, kLogoColor, kLogoAlpha, logoOpacity);
  drawText(c, kFontUi, kLeft, c.h - 22, kInk, BOOT_BYLINE, logoOpacity);
  sparkle(c, c.w - 60, 20, 3, kPink);
  if (ms > 600) blocks(c, (c.w - 300) / 2, 262, 300, ms);
}

inline void drawMessage(Canvas &c, const char *title, const char *text, int32_t spinnerMs) {
  window(c, "Screenclip");
  drawText(c, kFontUiBig, kLeft, 150, kInk, title);
  if (text) drawText(c, kFontUi, kLeft, 202, kInk, text);
  if (spinnerMs >= 0) blocks(c, kLeft, 244, 420, spinnerMs);
}

inline void drawHelp(Canvas &c) {
  static const char *const kRows[][2] = {
      {"Un appui", "image suivante"},
      {"Appui long", "image précédente"},
      {"Deux appuis", "tourne l'écran"},
      {"Appui de 2 s", "ouvre les réglages"},
      {"Page Téléphone", "ajoute ou supprime tes écrans"},
      {"Sur un ordinateur", "devient une clé USB"},
  };
  window(c, "Mode d'emploi");
  for (int i = 0; i < 6; ++i) {
    const int y = 92 + i * 41;
    drawText(c, kFontUi, kLeft, y, kPurple, kRows[i][0]);
    drawText(c, kFontUi, 300, y, kInk, kRows[i][1]);
  }
}

// Colonne de droite de la page Téléphone : le QR code occupe la gauche.
inline void drawPhone(Canvas &c, const char *title, const char *const *lines, int count) {
  window(c, title);
  c.fillRect(14, 62, 252, 252, kInk);
  for (int i = 0; i < count; ++i) drawText(c, kFontUi, 292, 112 + i * 44, kInk, lines[i]);
}

struct SettingRow {
  const char *label;
  const char *value;
};

inline void drawSettings(Canvas &c, const SettingRow *rows, int count, int selected) {
  window(c, "Réglages");
  for (int i = 0; i < count; ++i) {
    const int y = 110 + i * 50;
    const bool on = i == selected;
    if (on) c.fillRect(0, y - 34, c.w, 46, kInk);
    drawText(c, kFontUi, kLeft, y, on ? kPaper : kInk, rows[i].label);
    drawText(c, kFontUi, 370, y, on ? kPaper : kInk, rows[i].value);
  }
  drawText(c, kFontUi, kLeft, 300, kPurple, "Appui : ligne suivante   Appui long : changer");
}
}
