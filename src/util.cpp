#include "util.hpp"

namespace otto {

float regularPolyRadius(float sideLen, uint32_t numSides) {
  return sideLen / (2.0f * std::sin(M_PI / numSides));
}

float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

// Interpolate between two angles, assuming both angles are in the range 0-2pi.
float lerpAngular(float angle, float targetAngle, float t) {
  auto angleDiff = std::abs(targetAngle - angle);
  if (std::abs(angle - (targetAngle + TWO_PI)) < angleDiff) {
    targetAngle += TWO_PI;
  }
  return angle + (targetAngle - angle) * t;
}

float mapClamp(float x, float imin, float imax, float omin, float omax) {
  return std::max(omin, std::min(omax, (x - imin) / (imax - imin) * (omax - omin) + omin));
}

float mapUnitClamp(float x, float imin, float imax) {
  return std::max(0.0f, std::min(1.0f, (x - imin) / (imax - imin)));
}

void AngularParticle::step() {
  auto vel = (angle - anglePrev) * (1.0f - friction);

  anglePrev = angle;
  angle = angle + vel;

  // Wrap angle to the range 0-2pi. Assumes angle is not less than -2pi.
  auto wrappedAngle = std::fmod(angle + TWO_PI, TWO_PI);
  if (wrappedAngle != angle) {
    anglePrev += wrappedAngle - angle;
    angle = wrappedAngle;
  }
}

void AngularParticle::lerp(float targetAngle, float t) {
  angle = lerpAngular(angle, targetAngle, t);
}

std::pair<std::string, std::string> formatMebibytes(uint64_t bytes) {
  static const std::string suffixes[] = { "B", "KiB", "MiB", "GiB" };
  uint64_t place = std::log(static_cast<double>(bytes)) / std::log(1024.0);
  double units = bytes / std::pow(1024.0, place);
  return { formatNumber(units, 1), suffixes[std::min(place, 3ull)] };
}

static const double MS_TO_HRS  = 1.0 / (60.0 * 60.0 * 1000.0);
static const double MS_TO_MINS = 1.0 / (60.0 * 1000.0);
static const double MS_TO_SECS = 1.0 / 1000.0;

std::pair<std::string, std::string> formatMillis(uint64_t ms) {
  static const std::string suffixes[] = { "hrs", "mins", "secs" };
  static const double conversions[] = { MS_TO_HRS, MS_TO_MINS, MS_TO_SECS };
  for (int i = 0; i < 3; ++i) {
    auto units = ms * conversions[i];
    if (units >= 1.0) return { formatNumber(units, 1), suffixes[i] };
  }
  return { std::to_string(ms), "ms" };
}

} // otto
