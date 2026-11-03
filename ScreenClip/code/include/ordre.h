#pragma once

#include <algorithm>
#include <vector>

// Range les fichiers selon la liste voulue. Les fichiers absents de cette liste,
// comme ceux copiés par USB, restent à la fin dans leur ordre actuel.
template <typename Path>
void applyOrder(std::vector<Path> &files, const std::vector<Path> &wanted) {
  auto rank = [&](const Path &file) { return std::find(wanted.begin(), wanted.end(), file) - wanted.begin(); };
  std::stable_sort(files.begin(), files.end(), [&](const Path &a, const Path &b) { return rank(a) < rank(b); });
}
