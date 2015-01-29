#include "gfx.hpp"
#include "glm.hpp"
#include "gtx/rotate_vector.hpp"
#include "nanosvg.h"
#include "choreograph/Choreograph.h"

#include <iostream>
#include <vector>

extern "C" {
  int init();
  int shutdown();
  int update();
  int rotary_changed(int delta);
}

static const float screenWidth  = 96.0f;
static const float screenHeight = 96.0f;

static ch::Timeline timeline;
static ch::Output<float> rotation;

struct Tile {
  glm::vec2 position;
  float rotation;
  NSVGimage image;
};

static float regularPolyRadius(float sideLen, uint32_t numSides) {
  return sideLen / (2.0f * std::sin(M_PI / numSides));
}

static std::array<Tile, 6> tiles;
static float tileDiameter = screenWidth * 0.95f;
static float wheelEdgeLen = screenWidth * 1.1f;
static float wheelRadius = regularPolyRadius(wheelEdgeLen, tiles.size());

int init() {
  // Init tiles
  {
    float angle = 0.0f;
    float angleIncr = (M_PI * 2.0f) / tiles.size();
    for (auto &tile : tiles) {
      tile.rotation = angle;
      tile.position = glm::rotate(glm::vec2(0.0f, -wheelRadius), angle);
      angle += angleIncr;
    }
  }

  return 0;
}

int shutdown() {
  return 0;
}

int update() {
  using namespace otto;

  static const float defaultMatrix[] =
    { -0.0f, -1.0f, -0.0f, -1.0f,  0.0f,  0.0f, screenWidth, screenHeight, 1.0f };

  static const VGfloat bgColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

  timeline.step(1.0f / 30.0f);

  vgSetfv(VG_CLEAR_COLOR, 4, bgColor);
  vgClear(0, 0, 96, 96);

  vgLoadMatrix(defaultMatrix);
  vgTranslate(48, 48);
  vgTranslate(0, wheelRadius);
  vgRotate(rotation);

  fillColor(0, 1, 1);
  for (const auto &tile : tiles) {
    beginPath();
    arc(tile.position.x, tile.position.y, tileDiameter, tileDiameter, 0, M_PI * 2.0f);
    fill();
  }

  return 0;
}

int rotary_changed(int delta) {
  timeline.apply(&rotation).then<ch::RampTo>(rotation() + delta * 10.0f, 0.1f);
  return 0;
}
