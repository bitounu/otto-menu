#pragma once

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

std::pair<std::string, std::string> formatMebibytes(uint64_t bytes);

} // otto
