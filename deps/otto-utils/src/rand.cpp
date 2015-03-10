#include "rand.hpp"

#include <random>

namespace otto {

namespace rand {

static std::mt19937 base;
static std::uniform_real_distribution<float> floatGen;

} // rand

int32_t randInt() {
  return rand::base();
}

int32_t randInt(int32_t v) {
  if (v <= 0)
    return 0;
  else
    return rand::base() % v;
}

int32_t randInt(int32_t a, int32_t b) {
  return randInt(b - a) + a;
}

float randFloat() {
  return rand::floatGen(rand::base);
}

float randFloat(float v) {
  return rand::floatGen(rand::base) * v;
}

float randFloat(float a, float b) {
  return rand::floatGen(rand::base) * (b - a) + a;
}

vec2 randVec2(const Rect &rect) {
  auto x = randFloat(rect.getArea());
  return rect.pos + vec2(std::fmod(x, rect.size.x), x / rect.size.x);
}

} // otto
