#pragma once

#include <Arduino.h>

#include <algorithm>

#include "ecran.h"
#include "reglages.h"

// Copie une image source (GIF ou menu) vers l'écran, mise à l'échelle et tournée.
namespace rendu {
inline RgbDisplay display;
inline void (*overlay)(uint16_t *panel) = nullptr;

inline const uint16_t *srcPixels = nullptr;
inline int gifSrcW = 1, gifSrcH = 1;
inline int gifDstW = 1, gifDstH = 1;
inline bool srcCover = true;

inline uint32_t rowMap[PANEL_H];
inline uint32_t colMap[PANEL_W];

constexpr int kTileCols = 64;
inline uint16_t tile[PANEL_W * kTileCols];
inline uint16_t tileRow[PANEL_W];
inline uint32_t tileSrcRow[PANEL_W];
inline int tileRows = 0;

inline int rowFirst = 0, rowEnd = PANEL_H, colFirst = 0, colEnd = PANEL_W;

inline int rotation = SCREEN_ROTATION;
inline int screenW() { return (rotation & 1) ? PANEL_H : PANEL_W; }
inline int screenH() { return (rotation & 1) ? PANEL_W : PANEL_H; }

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline void buildMaps() {
  const int sw = screenW();
  const int sh = screenH();
  if ((gifSrcW * sh < gifSrcH * sw) == srcCover) {
    gifDstW = sw;
    gifDstH = gifSrcH * sw / gifSrcW;
  } else {
    gifDstH = sh;
    gifDstW = gifSrcW * sh / gifSrcH;
  }
  const int offX = (sw - gifDstW) / 2;
  const int offY = (sh - gifDstH) / 2;
  auto srcX = [&](int sx) { return (uint32_t)clampi((sx - offX) * gifSrcW / gifDstW, 0, gifSrcW - 1); };
  auto srcY = [&](int sy) { return (uint32_t)clampi((sy - offY) * gifSrcH / gifDstH, 0, gifSrcH - 1) * gifSrcW; };

  for (int py = 0; py < PANEL_H; ++py) {
    switch (rotation & 3) {
      case 0: rowMap[py] = srcY(py); break;
      case 1: rowMap[py] = srcX(py); break;
      case 2: rowMap[py] = srcY(PANEL_H - 1 - py); break;
      default: rowMap[py] = srcX(PANEL_H - 1 - py); break;
    }
  }
  for (int px = 0; px < PANEL_W; ++px) {
    switch (rotation & 3) {
      case 0: colMap[px] = srcX(px); break;
      case 1: colMap[px] = srcY(PANEL_W - 1 - px); break;
      case 2: colMap[px] = srcX(PANEL_W - 1 - px); break;
      default: colMap[px] = srcY(px); break;
    }
  }

  const int x0 = clampi(offX, 0, sw), x1 = clampi(offX + gifDstW, 0, sw);
  const int y0 = clampi(offY, 0, sh), y1 = clampi(offY + gifDstH, 0, sh);
  switch (rotation & 3) {
    case 0: rowFirst = y0; rowEnd = y1; colFirst = x0; colEnd = x1; break;
    case 1: rowFirst = x0; rowEnd = x1; colFirst = PANEL_W - y1; colEnd = PANEL_W - y0; break;
    case 2: rowFirst = PANEL_H - y1; rowEnd = PANEL_H - y0; colFirst = PANEL_W - x1; colEnd = PANEL_W - x0; break;
    default: rowFirst = PANEL_H - x1; rowEnd = PANEL_H - x0; colFirst = y0; colEnd = y1; break;
  }

  tileRows = 0;
  for (int px = 0; px < PANEL_W; ++px) {
    if (px == 0 || colMap[px] != colMap[px - 1]) tileSrcRow[tileRows++] = colMap[px];
    tileRow[px] = tileRows - 1;
  }
}

inline void renderRotated(const uint16_t *canvas, uint16_t *dst) {
  int py = 0;
  while (py < PANEL_H) {
    uint32_t cmin = rowMap[py], cmax = rowMap[py];
    int end = py + 1;
    while (end < PANEL_H) {
      const uint32_t lo = std::min(cmin, rowMap[end]);
      const uint32_t hi = std::max(cmax, rowMap[end]);
      if (hi - lo >= kTileCols) break;
      cmin = lo;
      cmax = hi;
      ++end;
    }
    const int ncols = cmax - cmin + 1;
    for (int r = 0; r < tileRows; ++r) {
      memcpy(tile + r * kTileCols, canvas + tileSrcRow[r] + cmin, ncols * 2);
    }
    for (; py < end; ++py) {
      uint16_t *d = dst + py * PANEL_W;
      if (py > 0 && rowMap[py] == rowMap[py - 1]) {
        memcpy(d, d - PANEL_W, PANEL_W * 2);
        continue;
      }
      const uint16_t *col = tile + (rowMap[py] - cmin);
      for (int px = 0; px < PANEL_W; ++px) d[px] = col[tileRow[px] * kTileCols];
    }
  }
}

inline void blackBars(uint16_t *dst) {
  if (rowFirst > 0) memset(dst, 0, rowFirst * PANEL_W * 2);
  if (rowEnd < PANEL_H) memset(dst + rowEnd * PANEL_W, 0, (PANEL_H - rowEnd) * PANEL_W * 2);
  if (colFirst > 0 || colEnd < PANEL_W) {
    for (int py = rowFirst; py < rowEnd; ++py) {
      uint16_t *d = dst + py * PANEL_W;
      memset(d, 0, colFirst * 2);
      memset(d + colEnd, 0, (PANEL_W - colEnd) * 2);
    }
  }
}

inline void render() {
  if (rotation & 1) {
    uint16_t *back = display.backBuffer();
    renderRotated(srcPixels, back);
    blackBars(back);
    if (overlay) overlay(back);
    display.present();
    return;
  }
  const uint16_t *canvas = srcPixels;
  uint16_t *dst = display.backBuffer();
  for (int py = 0; py < PANEL_H; ++py) {
    uint16_t *d = dst + py * PANEL_W;

    if (py > 0 && rowMap[py] == rowMap[py - 1]) {
      memcpy(d, d - PANEL_W, PANEL_W * 2);
      continue;
    }
    const uint16_t *src = canvas + rowMap[py];
    for (int px = 0; px < PANEL_W; ++px) d[px] = src[colMap[px]];
  }
  blackBars(dst);
  if (overlay) overlay(dst);
  display.present();
}

inline bool hasSource() { return srcPixels != nullptr; }

inline void clearSource() { srcPixels = nullptr; }

// cover : remplit l'écran quitte à recadrer, sinon l'image entière reste visible.
inline void setSource(const uint16_t *pixels, int w, int h, bool cover) {
  srcPixels = pixels;
  gifSrcW = w;
  gifSrcH = h;
  srcCover = cover;
  buildMaps();
}

inline void setRotation(int value) {
  rotation = value & 3;
  if (srcPixels) {
    buildMaps();
    render();
  }
}
}
