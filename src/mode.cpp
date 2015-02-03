#include "gfx.hpp"
#include "gtx/rotate_vector.hpp"
#include "nanosvg.h"
#include "choreograph/Choreograph.h"

#include <iostream>
#include <vector>
#include <chrono>

#define STAK_EXPORT extern "C"

using namespace glm;
using namespace choreograph;

using fsecs = std::chrono::duration<float>;

static const float TWO_PI = M_PI * 2.0f;

static const float screenWidth = 96.0f;
static const float screenHeight = 96.0f;

static float regularPolyRadius(float sideLen, uint32_t numSides) {
  return sideLen / (2.0f * std::sin(M_PI / numSides));
}

static float tileDiameter = screenWidth * 0.95f;
static float wheelEdgeLen = screenWidth * 1.1f;
static float wheelRadius = regularPolyRadius(wheelEdgeLen, 6);

static bool wheelIsMoving = false;

static const vec3 tileDefaultColor = { 0, 1, 1 };

struct Tile {
  Output<vec3> color = tileDefaultColor;
  std::shared_ptr<NSVGimage> numberSvg;
};

struct ModeData {
  ch::Timeline timeline;

  float angle = 0.0f;
  float anglePrev = angle;

  std::chrono::steady_clock::time_point lastFrameTime;
  std::chrono::steady_clock::time_point lastCrankTime;

  std::array<Tile, 6> tiles;
};

static ModeData data;

STAK_EXPORT int init() {
  // Load number graphics
  for (int i = 0; i < data.tiles.size(); ++i) {
    auto filename = "assets/" + std::to_string(i + 1) + ".svg";
    auto img = nsvgParseFromFile(filename.c_str(), "px", 96);
    data.tiles[i].numberSvg = std::shared_ptr<NSVGimage>(img, nsvgDelete);
  }

  data.lastFrameTime = std::chrono::steady_clock::now();

  return 0;
}

STAK_EXPORT int shutdown() { return 0; }

STAK_EXPORT int update() {
  using namespace otto;
  using namespace std::chrono;

  static const mat3 defaultMatrix{ 0.0f,        -1.0f,        -0.0f,
                                   -1.0f,       0.0f,         0.0f,
                                   screenWidth, screenHeight, 1.0f };

  auto frameTime = steady_clock::now();
  auto dt = duration_cast<fsecs>(frameTime - data.lastFrameTime).count();

  data.timeline.step(dt);

  clearColor(0, 0, 0);
  clear(0, 0, 96, 96);

  setTransform(defaultMatrix);
  translate(48, 48);

  translate(wheelRadius, 0.0f);
  rotate(data.angle);

  strokeWidth(2.0f);
  strokeColor(1, 0, 0);

  float angleIncr = -TWO_PI / 6.0f;
  for (const auto &tile : data.tiles) {
    pushTransform();
      translate(vec2(-wheelRadius, 0.0f));

      beginPath();
      circle(vec2(), tileDiameter * 0.5f);
      fillColor(tile.color());
      fill();

      translate(-48, -48);
      draw(tile.numberSvg.get());
    popTransform();

    rotate(angleIncr);
  }

  float vel = (data.angle - data.anglePrev) * 0.8f;
  data.anglePrev = data.angle;
  data.angle = data.angle + vel;

  // Wrap angle to the range 0-2pi. Assumes angle is not less than -2pi.
  float wrappedAngle = std::fmod(data.angle + TWO_PI, TWO_PI);
  if (wrappedAngle != data.angle) {
    data.anglePrev += wrappedAngle - data.angle;
    data.angle = wrappedAngle;
  }

  auto timeSinceLastCrank = frameTime - data.lastCrankTime;
  if (timeSinceLastCrank > std::chrono::milliseconds(500)) {
    int tileIndex = std::fmod(std::round(data.angle / TWO_PI * 6.0f), 6.0f);

    data.timeline.apply(&data.tiles[tileIndex].color)
        .then<RampTo>(vec3(1, 1, 0), 0.2f);

    float goalAngle = tileIndex / 6.0f * TWO_PI;
    float goalAngleDiff = std::abs(data.angle - goalAngle);
    if (std::abs(data.angle - (goalAngle + TWO_PI)) < goalAngleDiff) {
      goalAngle += TWO_PI;
    }
    data.angle = data.angle + (goalAngle - data.angle) * 0.2f;

    wheelIsMoving = false;
  }

  data.lastFrameTime = frameTime;

  return 0;
}

STAK_EXPORT int rotary_changed(int delta) {
  data.angle += delta * 0.02f;
  data.lastCrankTime = std::chrono::steady_clock::now();

  if (!wheelIsMoving) {
    wheelIsMoving = true;
    for (auto &tile : data.tiles) {
      data.timeline.apply(&tile.color).then<RampTo>(tileDefaultColor, 1.0f);
    }
  }

  return 0;
}
