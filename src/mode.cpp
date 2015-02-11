#include "gfx.hpp"
#include "gtx/rotate_vector.hpp"
#include "nanosvg.h"
#include "choreograph/Choreograph.h"
#include "stak.h"

#include <iostream>
#include <vector>
#include <chrono>
#include <functional>

#define STAK_EXPORT extern "C"

using namespace glm;
using namespace choreograph;
using namespace otto;

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

  void lerp(float targetAngle, float t) { angle = lerpAngular(angle, targetAngle, t); }
};

struct Carousel {
  AngularParticle rotation;

  uint32_t tileCount = 2;
  float tileRadius = 48.0f;

  Carousel() { rotation.friction = 0.2f; }

  uint32_t getActiveTileIndex() {
    return std::fmod(std::round(rotation.angle / TWO_PI * tileCount), tileCount);
  }

  void turn(float amount) { rotation.angle += amount / tileCount; }

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

class Menu;

struct MenuItem {
  Output<vec3> color = tileDefaultColor;
  Output<float> scale = 1.0f;

  static void defaultDraw(const MenuItem &item) {
    beginPath();
    circle(vec2(), 44);
    fillColor(item.color());
    fill();
  }
  static void defaultAction(const MenuItem &item) {}

  std::function<void(const MenuItem &)> draw = defaultDraw;
  std::function<void(const MenuItem &)> action = defaultAction;

  std::unique_ptr<Menu> subMenu;
};

struct Menu {
  Carousel carousel;

  std::vector<std::unique_ptr<MenuItem>> items;
  MenuItem *activeItem = nullptr;
};

struct TestMode {
  ch::Timeline timeline;

  std::unique_ptr<Menu> rootMenu;
  Menu *activeMenu = nullptr;

  std::chrono::steady_clock::time_point lastCrankTime;

  float secondsPerFrame;
  uint32_t frameCount = 0;
};


static TestMode mode;

STAK_EXPORT int init() {
  loadFont("assets/232MKSD-round-light.ttf");

  mode.rootMenu = std::make_unique<Menu>();
  mode.activeMenu = mode.rootMenu.get();

  auto makeItem = [&] {
    mode.rootMenu->items.emplace_back(new MenuItem());
    mode.rootMenu->carousel.tileCount = mode.rootMenu->items.size();
    return mode.rootMenu->items.back().get();
  };

  {
    auto wifi = makeItem();
    wifi->draw = [](const MenuItem &item) {
      MenuItem::defaultDraw(item);
      fontSize(34);
      textAlign(ALIGN_MIDDLE | ALIGN_CENTER);
      fillColor(0, 0, 0);
      fillText("wifi");
    };
  }

  {
    auto item = makeItem();
    item->draw = [](const MenuItem &item) {
      MenuItem::defaultDraw(item);
      fontSize(28);
      textAlign(ALIGN_MIDDLE | ALIGN_CENTER);
      fillColor(0, 0, 0);
      fillText("mode");
    };
  }

  {
    auto item = makeItem();
    item->draw = [](const MenuItem &item) {
      MenuItem::defaultDraw(item);
      fontSize(34);
      textAlign(ALIGN_MIDDLE | ALIGN_CENTER);
      fillColor(0, 0, 0);
      fillText("off");
    };
  }

  return 0;
}

STAK_EXPORT int shutdown() {
  return 0;
}

STAK_EXPORT int update(float dt) {
  auto &menu = *mode.activeMenu;

  mode.timeline.step(dt);
  menu.carousel.step();

  auto timeSinceLastCrank = std::chrono::steady_clock::now() - mode.lastCrankTime;
  if (timeSinceLastCrank > std::chrono::milliseconds(400)) {
    auto tileIndex = menu.carousel.getActiveTileIndex();

    if (!menu.activeItem) {
      menu.activeItem = menu.items[tileIndex].get();
      mode.timeline.apply(&menu.activeItem->color)
          .then<RampTo>(tileActiveColor, 0.2f, EaseOutQuad());
      mode.timeline.apply(&menu.activeItem->scale).then<RampTo>(1.0f, 0.2f, EaseOutQuad());
    }

    menu.carousel.rotation.lerp(float(tileIndex) / menu.carousel.tileCount * TWO_PI, 0.2f);
  }

  mode.frameCount++;

  mode.secondsPerFrame += dt;
  if (mode.frameCount % 60 == 0) {
    std::cout << (1.0f / (mode.secondsPerFrame / 60.0f)) << " fps" << std::endl;
    mode.secondsPerFrame = 0.0f;
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

  mode.activeMenu->carousel.draw([&](int i) {
    const auto &item = *mode.activeMenu->items[i];
    scale(item.scale());
    item.draw(item);
  });

  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  auto &menu = *mode.activeMenu;

  menu.carousel.turn(amount * 0.1f);
  mode.lastCrankTime = std::chrono::steady_clock::now();

  if (menu.activeItem) {
    menu.activeItem = nullptr;
    for (auto &item : menu.items) {
      mode.timeline.apply(&item->color).then<RampTo>(tileDefaultColor, 0.4f, EaseInOutQuad());
      mode.timeline.apply(&item->scale).then<RampTo>(0.7f, 0.4f, EaseInOutQuad());
    }
  }

  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  std::cout << "pressed" << std::endl;

  auto activeItem = mode.activeMenu->activeItem;
  if (activeItem) {
    mode.timeline.apply(&activeItem->scale).then<RampTo>(0.75f, 0.25f, EaseOutQuad());
    mode.timeline.apply(&activeItem->color).then<RampTo>(vec3(1, 0, 0), 0.25f, EaseOutQuad());
  }

  return 0;
}

STAK_EXPORT int shutter_button_released() {
  // stak_activate_mode();

  std::cout << "released" << std::endl;

  auto activeItem = mode.activeMenu->activeItem;
  if (activeItem) {
    mode.timeline.apply(&activeItem->scale).then<RampTo>(1.0f, 0.25f, EaseInQuad());
    mode.timeline.apply(&activeItem->color).then<RampTo>(tileActiveColor, 0.25f, EaseInQuad());
  }

  return 0;
}

STAK_EXPORT int crank_pressed() {
  std::cout << "crank pressed" << std::endl;
  return 0;
}

STAK_EXPORT int crank_released() {
  std::cout << "crank released" << std::endl;
  return 0;
}
