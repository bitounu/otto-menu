#pragma once

#include <sstream>
#include <iomanip>

namespace otto {

const float TWO_PI = M_PI * 2.0f;

float regularPolyRadius(float sideLen, uint32_t numSides);

float lerpAngular(float angle, float targetAngle, float t);

struct AngularParticle {
  float angle = 0.0f;
  float anglePrev = angle;

  float friction = 0.0f;

  void step();
  void lerp(float targetAngle, float t);
};

template <typename T>
std::string formatNumber(T x, int precision) {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(precision) << x;
  return ss.str();
}

std::pair<std::string, std::string> formatMebibytes(uint64_t bytes);
std::pair<std::string, std::string> formatMillis(uint64_t ms);

} // otto
