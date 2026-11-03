// Lancer depuis code/ : g++ -std=c++17 -Iinclude tests/ordre.cpp -o /tmp/ordre && /tmp/ordre
#include <cassert>
#include <cstdio>
#include <string>

#include "ordre.h"

using Paths = std::vector<std::string>;

int main() {
  Paths files = {"a", "b", "c", "d"};
  applyOrder(files, {"c", "a"});
  assert((files == Paths{"c", "a", "b", "d"}));

  Paths gone = {"a", "b"};
  applyOrder(gone, {"supprimé", "b"});
  assert((gone == Paths{"b", "a"}));

  Paths same = {"a", "b"};
  applyOrder(same, {});
  assert((same == Paths{"a", "b"}));
  puts("ok");
}
