#include "gfx.hpp"
#include "gtx/rotate_vector.hpp"
#include "nanosvg.h"
#include "choreograph/Choreograph.h"

#include <iostream>
#include <vector>
#include <chrono>
#include <functional>

#define STAK_EXPORT extern "C"

using namespace glm;
using namespace choreograph;
using namespace otto;

using fsecs = std::chrono::duration<float>;

static const float TWO_PI = M_PI * 2.0f;

static const float screenWidth = 96.0f;
static const float screenHeight = 96.0f;

static const vec3 tileDefaultColor = { 0, 1, 1 };
static const vec3 tileActiveColor = { 1, 1, 0 };

static float regularPolyRadius(float sideLen, uint32_t numSides) {
  return sideLen / (2.0f * std::sin(M_PI / numSides));
}

// Interpolate between two angles, assuming both angles are in the range 0-2pi.
static float lerpAngular(float angle, float targetAngle, float t) {
  auto angleDiff = std::abs(targetAngle - angle);
  if (std::abs(angle - (targetAngle + TWO_PI)) < angleDiff) {
    targetAngle += TWO_PI;
  }
  return angle + (targetAngle - angle) * t;
}

struct AngularParticle {
  float angle = 0.0f;
  float anglePrev = angle;

  float friction = 0.0f;

  void step() {
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

  void lerp(float targetAngle, float t) {
    angle = lerpAngular(angle, targetAngle, t);
  }
};

struct Tile {
  Output<vec3> color = tileDefaultColor;
  Output<float> scale = 1.0f;
};

struct Carousel {
  AngularParticle rotation;

  uint32_t tileCount = 6;
  float tileRadius = 48.0f;

  Carousel() { rotation.friction = 0.2f; }

  uint32_t getActiveTileIndex() {
    return std::fmod(std::round(rotation.angle / TWO_PI * tileCount), tileCount);
  }

  void turn(float amount) {
    rotation.angle += amount / tileCount;
  }

  void step() { rotation.step(); }

  void draw(const std::function<void(int i)> &drawTileFn) {
    auto radius = regularPolyRadius(tileRadius * 2.2f, tileCount);
    auto angleIncr = -TWO_PI / tileCount;

    pushTransform();
      translate(radius, 0.0f);
      rotate(rotation.angle);
      for (int i = 0; i < tileCount; ++i) {
        pushTransform();
          translate(-radius, 0.0f);
          drawTileFn(i);
        popTransform();
        rotate(angleIncr);
      }
    popTransform();
  }
};

struct ModeData {
  ch::Timeline timeline;

  Carousel carousel;

  std::chrono::steady_clock::time_point lastCrankTime;

  float secondsPerFrame;
  uint32_t frameCount = 0;

  std::array<Tile, 6> tiles;
  Tile *activeTile = nullptr;
};

static ModeData data;

STAK_EXPORT int init() {
  loadFont("assets/232MKSD-round-light.ttf");

  return 0;
}

STAK_EXPORT int shutdown() { return 0; }

STAK_EXPORT int update(float dt) {
  data.timeline.step(dt);
  data.carousel.step();

  auto timeSinceLastCrank = std::chrono::steady_clock::now() - data.lastCrankTime;
  if (timeSinceLastCrank > std::chrono::milliseconds(400)) {
    auto tileIndex = data.carousel.getActiveTileIndex();

    if (!data.activeTile) {
      data.activeTile = &data.tiles[tileIndex];
      data.timeline.apply(&data.activeTile->color)
          .then<RampTo>(tileActiveColor, 0.2f, EaseOutQuad());
      data.timeline.apply(&data.activeTile->scale).then<RampTo>(1.0f, 0.2f, EaseOutQuad());
    }

    data.carousel.rotation.lerp(float(tileIndex) / data.carousel.tileCount * TWO_PI, 0.2f);
  }

  data.frameCount++;

  data.secondsPerFrame += dt;
  if (data.frameCount % 60 == 0) {
    std::cout << (1.0f / (data.secondsPerFrame / 60.0f)) << std::endl;
    data.secondsPerFrame = 0.0f;
  }

  return 0;
}

STAK_EXPORT int draw() {
  static const mat3 defaultMatrix{ 0.0f, -1.0f,       0.0f,         -1.0f, 0.0f,
                                   0.0f, screenWidth, screenHeight, 1.0f };

  clearColor(0, 0, 0);
  clear(0, 0, 96, 96);

  setTransform(defaultMatrix);
  translate(48, 48);

  data.carousel.draw([&](int i) {
    const auto &tile = data.tiles[i];

    scale(tile.scale());

    beginPath();
    circle(vec2(), 44);
    fillColor(tile.color());
    fill();

    fontSize(42);
    textAlign(ALIGN_MIDDLE | ALIGN_CENTER);
    fillColor(0, 0, 0);
    fillText(std::to_string(i + 1));
  });

  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  data.carousel.turn(amount * 0.1f);
  data.lastCrankTime = std::chrono::steady_clock::now();

  if (data.activeTile) {
    data.activeTile = nullptr;
    for (auto &tile : data.tiles) {
      data.timeline.apply(&tile.color).then<RampTo>(tileDefaultColor, 0.4f, EaseInOutQuad());
      data.timeline.apply(&tile.scale).then<RampTo>(0.7f, 0.4f, EaseInOutQuad());
    }
  }

  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  std::cout << "pressed" << std::endl;
  if (data.activeTile) {
    data.timeline.apply(&data.activeTile->scale).then<RampTo>(0.75f, 0.25f, EaseOutQuad());
    data.timeline.apply(&data.activeTile->color).then<RampTo>(vec3(1, 0, 0), 0.25f, EaseOutQuad());
  }

  return 0;
}

STAK_EXPORT int shutter_button_released() {
  std::cout << "released" << std::endl;
  if (data.activeTile) {
    data.timeline.apply(&data.activeTile->scale).then<RampTo>(1.0f, 0.25f, EaseInQuad());
    data.timeline.apply(&data.activeTile->color).then<RampTo>(tileActiveColor, 0.25f, EaseInQuad());
  }

  return 0;
}

STAK_EXPORT int power_button_pressed() {
  std::cout << "power pressed" << std::endl;
  return 0;
}

STAK_EXPORT int power_button_released() {
  std::cout << "power released" << std::endl;
  return 0;
}
