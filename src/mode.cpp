#include "gfx.hpp"
#include "gtx/rotate_vector.hpp"
#include "nanosvg.h"
#include "choreograph/Choreograph.h"

#include <iostream>
#include <vector>
#include <chrono>

extern "C" {
  int init();
  int shutdown();
  int update();
  int rotary_changed(int delta);
}

using namespace glm;

static const float TWO_PI = M_PI * 2.0f;

static const float screenWidth  = 96.0f;
static const float screenHeight = 96.0f;

static float regularPolyRadius(float sideLen, uint32_t numSides) {
  return sideLen / (2.0f * std::sin(M_PI / numSides));
}

static float tileDiameter = screenWidth * 0.95f;
static float wheelEdgeLen = screenWidth * 1.1f;
static float wheelRadius = regularPolyRadius(wheelEdgeLen, 6);

struct ModeData {
  ch::Timeline timeline;

  float angle = 0.0f;
  float anglePrev = angle;

  std::chrono::steady_clock::time_point lastCrankTime;

  std::array<std::shared_ptr<NSVGimage>, 6> numbers;
};

static ModeData data;

int init() {
  // Load number graphics
  for (int i = 0; i < data.numbers.size(); ++i) {
    auto filename = "assets/" + std::to_string(i + 1) + ".svg";
    auto img = nsvgParseFromFile(filename.c_str(), "px", 96);
    data.numbers[i] = std::shared_ptr<NSVGimage>(img, nsvgDelete);
  }

  return 0;
}

int shutdown() {
  return 0;
}

int update() {
  using namespace otto;

  static const mat3 defaultMatrix{ -0.0f, -1.0f, -0.0f, -1.0f, 0.0f, 0.0f, screenWidth, screenHeight, 1.0f };
  static const VGfloat bgColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

  data.timeline.step(1.0f / 30.0f);

  vgSetfv(VG_CLEAR_COLOR, 4, bgColor);
  vgClear(0, 0, 96, 96);

  setTransform(defaultMatrix);
  translate(48, 48);

  translate(wheelRadius, 0);
  rotate(data.angle);

  strokeWidth(2.0f);
  strokeColor(1, 0, 0);

  for (const auto &number : data.numbers) {
    pushTransform();
      translate(vec2(-wheelRadius, 0.0f));

      beginPath();
      circle(vec2(), tileDiameter * 0.5f);
      fillColor(0, 1, 0.5);
      fill();

      translate(-48, -48);
      draw(number.get());
    popTransform();

    rotate(TWO_PI / 6.0f);
  }

  float vel = (data.angle - data.anglePrev) * 0.8f;
  data.anglePrev = data.angle;
  data.angle += vel;

  auto timeSinceLastCrank = std::chrono::steady_clock::now() - data.lastCrankTime;
  if (timeSinceLastCrank > std::chrono::milliseconds(500)) {
    float goal = std::round(data.angle / TWO_PI * 6.0f) / 6.0f * TWO_PI;
    data.angle = data.angle + (goal - data.angle) * 0.2f;
  }

  return 0;
}

int rotary_changed(int delta) {
  data.angle += delta * 0.02f;
  data.lastCrankTime = std::chrono::steady_clock::now();

  return 0;
}
