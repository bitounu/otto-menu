#include "gfx.hpp"

#include <iostream>

extern "C" {
  int init();
  int shutdown();
  int update();
}

int init() {
  std::cout << "HELLO!!!" << std::endl;
  return 0;
}

int shutdown() {
  return 0;
}

int update() {
  return 0;
}
