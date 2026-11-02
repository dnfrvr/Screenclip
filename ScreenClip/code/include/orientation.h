#pragma once

#include <math.h>
#include <stdint.h>

// Logique pure, sans accès au matériel : testable sur ordinateur.
namespace orientation {
// Trouve de quel côté est la barrette (0 : +X, 1 : +Y, 2 : -X, 3 : -Y), à partir
// de la gravité en g. Renvoie -1 tant qu'aucune position n'est confirmée.
class Tracker {
 public:
  Tracker(float minTilt, float dominance, uint32_t holdMs)
      : minTilt_(minTilt), dominance_(dominance), holdMs_(holdMs) {}

  int update(float ax, float ay, uint32_t nowMs) {
    const int seen = classify(ax, ay);
    if (seen != candidate_) {
      candidate_ = seen;
      since_ = nowMs;
    }
    if (candidate_ >= 0 && nowMs - since_ >= holdMs_) stable_ = candidate_;
    return stable_;
  }

 private:
  int classify(float ax, float ay) const {
    const float x = fabsf(ax), y = fabsf(ay);
    if (sqrtf(ax * ax + ay * ay) < minTilt_) return -1;
    if (x >= y * dominance_) return ax > 0 ? 0 : 2;
    if (y >= x * dominance_) return ay > 0 ? 1 : 3;
    return -1;
  }

  float minTilt_, dominance_;
  uint32_t holdMs_;
  int candidate_ = -1, stable_ = -1;
  uint32_t since_ = 0;
};

// Détecte un mouvement en comparant l'accélération à sa moyenne glissante.
class Motion {
 public:
  explicit Motion(float threshold) : threshold_(threshold) {}

  bool update(float ax, float ay, float az) {
    if (!seeded_) {
      x_ = ax; y_ = ay; z_ = az;
      seeded_ = true;
      return false;
    }
    const float dx = ax - x_, dy = ay - y_, dz = az - z_;
    const bool moved = sqrtf(dx * dx + dy * dy + dz * dz) > threshold_;
    x_ += dx * 0.1f; y_ += dy * 0.1f; z_ += dz * 0.1f;
    return moved;
  }

 private:
  float threshold_, x_ = 0, y_ = 0, z_ = 0;
  bool seeded_ = false;
};
}
