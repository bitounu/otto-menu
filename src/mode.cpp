#include "gfx.hpp"
#include "gtx/rotate_vector.hpp"
#include "nanosvg.h"
#include "choreograph/Choreograph.h"
#include "stak.h"

#include <vector>
#include <chrono>
#include <functional>

// Debug
#include <iostream>

#define STAK_EXPORT extern "C"

using namespace glm;
using namespace choreograph;
using namespace otto;

static const float TWO_PI = M_PI * 2.0f;

static const float screenWidth = 96.0f;
static const float screenHeight = 96.0f;

static const vec3 tileDefaultColor = { 0, 1, 1 };
static const vec3 tileActiveColor = { 1, 1, 0 };

static Timeline timeline;

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

  void draw(const std::function<void(int i)> &drawTile) const {
    auto radius = regularPolyRadius(tileRadius * 2.2f, tileCount);
    auto angleIncr = -TWO_PI / tileCount;

    pushTransform();
    translate(radius, 0.0f);
    rotate(rotation.angle);
    for (int i = 0; i < tileCount; ++i) {
      pushTransform();
      translate(-radius, 0.0f);
      drawTile(i);
      popTransform();
      rotate(angleIncr);
    }
    popTransform();
  }
};

class Menu;

static void activateMenu(Menu *menu, bool pushToStack = true);

struct MenuItem {
  Output<vec3> color = tileDefaultColor;
  Output<float> scale = 0.7f;

  static void defaultHandleDraw(const MenuItem &item) {
    beginPath();
    circle(vec2(), 44);
    fillColor(item.color());
    fill();
  }

  static void defaultHandleSelect(MenuItem &item) {
    timeline.apply(&item.color).then<RampTo>(tileActiveColor, 0.2f, EaseInOutQuad());
    timeline.apply(&item.scale).then<RampTo>(1.0f, 0.2f, EaseInOutQuad());
  }

  static void defaultHandleDeselect(MenuItem &item) {
    timeline.apply(&item.color).then<RampTo>(tileDefaultColor, 0.2f, EaseInOutQuad());
    timeline.apply(&item.scale).then<RampTo>(0.7f, 0.2f, EaseInOutQuad());
  }

  static void defaultHandleActivate(MenuItem &item) {
    if (item.subMenu) activateMenu(item.subMenu.get());
  }

  std::function<void(const MenuItem &)> handleDraw = defaultHandleDraw;
  std::function<void(MenuItem &)> handleSelect = defaultHandleSelect;
  std::function<void(MenuItem &)> handleDeselect = defaultHandleDeselect;
  std::function<void(MenuItem &)> handleActivate = defaultHandleActivate;

  std::unique_ptr<Menu> subMenu;
};

struct Menu {
  Output<vec2> position;
  Carousel carousel;

  std::vector<std::unique_ptr<MenuItem>> items;
  MenuItem *activeItem = nullptr;

  MenuItem *makeItem() {
    items.emplace_back(new MenuItem());
    carousel.tileCount = items.size();
    return items.back().get();
  }
};

struct MenuMode {
  std::unique_ptr<Menu> rootMenu = std::make_unique<Menu>();

  std::vector<Menu *> menuStack;
  Menu *activeMenu = nullptr;
  Menu *deactivatingMenu = nullptr;

  std::chrono::steady_clock::time_point lastCrankTime;

  float secondsPerFrame;
  uint32_t frameCount = 0;
};

static MenuMode mode;

static void activateMenu(Menu *menu, bool pushToStack) {
  // NOTE(ryan): Bail if there's already an activation in progress. We do this here to make the
  // user-facing API less error prone.
  if (mode.deactivatingMenu) return;

  // Deactivate previously active menu and animate out
  {
    mode.deactivatingMenu = mode.activeMenu;

    timeline.apply(&mode.deactivatingMenu->position)
        .then<RampTo>(vec2(-screenWidth, 0.0f), 0.75f, EaseInOutQuad())
        .finishFn([&](Motion<vec2> &m) { mode.deactivatingMenu = nullptr; });

    if (pushToStack) {
      mode.menuStack.push_back(mode.deactivatingMenu);
    }
  }

  // Animate in the new active menu
  menu->position = vec2(screenWidth, 0.0f);
  timeline.apply(&menu->position).then<RampTo>(vec2(), 0.75f, EaseInOutQuad());
  mode.activeMenu = menu;
}

static void activatePreviousMenu() {
  if (!mode.deactivatingMenu && mode.menuStack.size()) {
    activateMenu(mode.menuStack.back(), false);
    mode.menuStack.pop_back();
  }
}

static void fillTextFitToWidth(const std::string &text, float width) {
  fontSize(1.0f);
  auto textWidth = getTextBounds(text).size.x;
  fontSize(width / textWidth);
  fillText(text);
}

STAK_EXPORT int init() {
  loadFont("assets/232MKSD-round-light.ttf");

  mode.activeMenu = mode.rootMenu.get();

  {
    auto wifi = mode.rootMenu->makeItem();

    wifi->handleDraw = [](const MenuItem &item) {
      MenuItem::defaultHandleDraw(item);

      textAlign(ALIGN_MIDDLE | ALIGN_CENTER);
      fillColor(0, 0, 0);
      fillTextFitToWidth("oTo", screenWidth * 0.65f);

      beginPath();
      arc(0, 0, 52, 52, M_PI * -0.3f, M_PI * -0.7f);
      strokeWidth(1.5f);
      strokeColor(0, 0, 0);
      stroke();
    };
    wifi->handleSelect = [](MenuItem &item) {
      MenuItem::defaultHandleSelect(item);
      std::cout << "wifi selected!" << std::endl;
    };
    wifi->handleActivate = [](MenuItem &item) {
      MenuItem::defaultHandleActivate(item);
      std::cout << "wifi activated!" << std::endl;
    };

    wifi->subMenu = std::make_unique<Menu>();
    auto a = wifi->subMenu->makeItem();
    auto b = wifi->subMenu->makeItem();
  }

  {
    auto item = mode.rootMenu->makeItem();
    item->handleDraw = [](const MenuItem &item) {
      MenuItem::defaultHandleDraw(item);
      textAlign(ALIGN_MIDDLE | ALIGN_CENTER);
      fillColor(0, 0, 0);
      fillTextFitToWidth("hello!", screenWidth * 0.65f);
    };
  }

  {
    auto item = mode.rootMenu->makeItem();
    item->handleDraw = [](const MenuItem &item) {
      MenuItem::defaultHandleDraw(item);
      textAlign(ALIGN_MIDDLE | ALIGN_CENTER);
      fillColor(0, 0, 0);
      fillTextFitToWidth("bye", screenWidth * 0.65f);
    };
  }

  for (auto &item : mode.rootMenu->items) {
    if (item->handleDeselect) item->handleDeselect(*item);
  }

  return 0;
}

STAK_EXPORT int shutdown() {
  return 0;
}

STAK_EXPORT int update(float dt) {
  auto &menu = *mode.activeMenu;

  timeline.step(dt);
  menu.carousel.step();

  auto timeSinceLastCrank = std::chrono::steady_clock::now() - mode.lastCrankTime;
  if (timeSinceLastCrank > std::chrono::milliseconds(350)) {
    auto tileIndex = menu.carousel.getActiveTileIndex();

    if (!menu.activeItem) {
      menu.activeItem = menu.items[tileIndex].get();
      if (menu.activeItem->handleSelect) menu.activeItem->handleSelect(*menu.activeItem);
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
  static const mat3 defaultMatrix{ 0.0, -1.0, 0.0, 1.0, -0.0, 0.0, 0.0, screenHeight, 1.0 };

  clearColor(0, 0, 0);
  clear(0, 0, 96, 96);

  setTransform(defaultMatrix);
  translate(48, 48);

  auto drawMenu = [&](const Menu &menu) {
    pushTransform();
    translate(menu.position());
    menu.carousel.draw([&](int i) {
      const auto &item = *menu.items[i];
      scale(item.scale());
      item.handleDraw(item);
    });
    popTransform();
  };

  if (mode.deactivatingMenu) drawMenu(*mode.deactivatingMenu);
  drawMenu(*mode.activeMenu);

  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  auto &menu = *mode.activeMenu;

  menu.carousel.turn(amount * -0.1f);
  mode.lastCrankTime = std::chrono::steady_clock::now();

  if (menu.activeItem) {
    if (menu.activeItem->handleDeselect) menu.activeItem->handleDeselect(*menu.activeItem);
    menu.activeItem = nullptr;
  }

  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  std::cout << "pressed" << std::endl;

  auto activeItem = mode.activeMenu->activeItem;
  if (activeItem) {
    timeline.apply(&activeItem->scale).then<RampTo>(0.75f, 0.25f, EaseOutQuad());
    timeline.apply(&activeItem->color).then<RampTo>(vec3(1, 0, 0), 0.25f, EaseOutQuad());
  }

  return 0;
}

STAK_EXPORT int shutter_button_released() {
  // stak_activate_mode();

  std::cout << "released" << std::endl;

  auto activeItem = mode.activeMenu->activeItem;
  if (activeItem) {
    if (activeItem->handleActivate) activeItem->handleActivate(*activeItem);
    timeline.apply(&activeItem->scale).then<RampTo>(1.0f, 0.25f, EaseInQuad());
    timeline.apply(&activeItem->color).then<RampTo>(tileActiveColor, 0.25f, EaseInQuad());
  }

  return 0;
}

STAK_EXPORT int power_button_pressed() {
  std::cout << "power pressed" << std::endl;
  activatePreviousMenu();
  return 0;
}

STAK_EXPORT int power_button_released() {
  std::cout << "power released" << std::endl;
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
