// Lancer depuis code/ : g++ -std=c++17 -Iinclude tests/orientation.cpp -o /tmp/orientation && /tmp/orientation
#include <cassert>
#include <cstdio>

#include "orientation.h"

using orientation::Motion;
using orientation::Tracker;

static void confirmsAfterHoldDelay() {
  Tracker t(0.5f, 1.4f, 600);
  assert(t.update(1, 0, 0) == -1);
  assert(t.update(1, 0, 599) == -1);
  assert(t.update(1, 0, 600) == 0);
}

static void keepsPositionWhenFlatOrDiagonal() {
  Tracker t(0.5f, 1.4f, 600);
  t.update(1, 0, 0);
  t.update(1, 0, 600);
  assert(t.update(0, 0.02f, 2000) == 0);
  assert(t.update(0.7f, 0.7f, 5000) == 0);
}

static void findsEachSide() {
  Tracker t(0.5f, 1.4f, 100);
  const float sides[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
  uint32_t now = 0;
  for (int side = 0; side < 4; ++side) {
    t.update(sides[side][0], sides[side][1], now);
    assert(t.update(sides[side][0], sides[side][1], now + 100) == side);
    now += 200;
  }
}

static void survivesClockOverflow() {
  Tracker t(0.5f, 1.4f, 600);
  t.update(1, 0, 0xFFFFFF00u);
  assert(t.update(1, 0, 0x200u) == 0);
}

static void detectsMotionButNotNoise() {
  Motion m(0.08f);
  assert(!m.update(0, 0, 1));
  for (int i = 0; i < 50; ++i) assert(!m.update(0.01f, 0, 1));
  assert(m.update(0.2f, 0, 1));
}

int main() {
  confirmsAfterHoldDelay();
  keepsPositionWhenFlatOrDiagonal();
  findsEachSide();
  survivesClockOverflow();
  detectsMotionButNotNoise();
  puts("ok");
}
