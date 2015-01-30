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

static ch::Timeline timeline;
static float angle = 0.0f, anglePrev = angle;

static std::chrono::steady_clock::time_point lastCrankTime;

static float regularPolyRadius(float sideLen, uint32_t numSides) {
  return sideLen / (2.0f * std::sin(M_PI / numSides));
}

static std::array<std::shared_ptr<NSVGimage>, 6> numbers;
// static std::array<Tile, 6> tiles;
static float tileDiameter = screenWidth * 0.95f;
static float wheelEdgeLen = screenWidth * 1.1f;
static float wheelRadius = regularPolyRadius(wheelEdgeLen, 6);

int init() {
  // Load number graphics
  for (int i = 0; i < numbers.size(); ++i) {
    auto filename = "assets/" + std::to_string(i + 1) + ".svg";
    auto img = nsvgParseFromFile(filename.c_str(), "px", 96);
    numbers[i] = std::shared_ptr<NSVGimage>(img, nsvgDelete);
  }

  // // Init tiles
  // {
  //   float angle = 0.0f;
  //   float angleIncr = (M_PI * 2.0f) / tiles.size();
  //   for (auto &tile : tiles) {
  //     tile.rotation = angle;
  //     tile.position = rotate(vec2(-wheelRadius, 0.0f), angle);
  //     angle += angleIncr;
  //   }
  // }

  return 0;
}

int shutdown() {
  return 0;
}

int update() {
  using namespace otto;

  static const mat3 defaultMatrix{ -0.0f, -1.0f, -0.0f, -1.0f, 0.0f, 0.0f, screenWidth, screenHeight, 1.0f };
  static const VGfloat bgColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

  timeline.step(1.0f / 30.0f);

  vgSetfv(VG_CLEAR_COLOR, 4, bgColor);
  vgClear(0, 0, 96, 96);

  setTransform(defaultMatrix);
  translate(vec2(48, 48));

  translate(vec2(wheelRadius, 0));
  rotate(angle);

  strokeWidth(2.0f);
  strokeColor(1, 0, 0);

  // beginPath();
  // moveTo(0, 0);
  // lineTo(-wheelRadius, 0);
  // stroke();

  for (const auto &number : numbers) {
    pushTransform();
      translate(vec2(-wheelRadius, 0.0f));

      beginPath();
      arc(0.0f, 0.0f, tileDiameter, tileDiameter, 0, M_PI * 2.0f);
      fillColor(0, 1, 0.5);
      fill();

      translate(vec2(-48, -48));
      draw(number.get());
    popTransform();

    rotate(TWO_PI / 6.0f);
  }

  float vel = (angle - anglePrev) * 0.8f;
  anglePrev = angle;
  angle += vel;

  auto timeSinceLastCrank = std::chrono::steady_clock::now() - lastCrankTime;
  if (timeSinceLastCrank > std::chrono::milliseconds(500)) {
    float goal = std::round(angle / TWO_PI * 6.0f) / 6.0f * TWO_PI;
    angle = angle + (goal - angle) * 0.2f;
  }

  return 0;
}

int rotary_changed(int delta) {
  angle += delta * 0.02f;

  lastCrankTime = std::chrono::steady_clock::now();
  // timeline.apply(&rotation).then<ch::RampTo>(rotation() + delta * 0.05f, 0.1f);
  return 0;
}
