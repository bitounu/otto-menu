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
    timeline.apply(&item.color).then<RampTo>(tileActiveColor, 0.2f, EaseOutQuad());
    timeline.apply(&item.scale).then<RampTo>(1.0f, 0.2f, EaseOutQuad());
  }
  static void defaultHandleDeselect(MenuItem &item) {
    timeline.apply(&item.color).then<RampTo>(tileDefaultColor, 0.2f, EaseInOutQuad());
    timeline.apply(&item.scale).then<RampTo>(0.7f, 0.2f, EaseInOutQuad());
  }

  static void defaultHandlePress(MenuItem &item) {
    timeline.apply(&item.scale).then<RampTo>(0.75f, 0.25f, EaseOutQuad());
    timeline.apply(&item.color).then<RampTo>(vec3(1, 0, 0), 0.25f, EaseOutQuad());
  }
  static void defaultHandleRelease(MenuItem &item) {
    timeline.apply(&item.scale).then<RampTo>(1.0f, 0.25f, EaseOutQuad());
    timeline.apply(&item.color).then<RampTo>(tileActiveColor, 0.25f, EaseOutQuad());
  }

  static void defaultHandleActivate(MenuItem &item) {
    if (item.subMenu) activateMenu(item.subMenu.get());
  }

  std::function<void(const MenuItem &)> handleDraw = defaultHandleDraw;
  std::function<void(MenuItem &)> handleSelect = defaultHandleSelect;
  std::function<void(MenuItem &)> handleDeselect = defaultHandleDeselect;
  std::function<void(MenuItem &)> handlePress = defaultHandlePress;
  std::function<void(MenuItem &)> handleRelease = defaultHandleRelease;
  std::function<void(MenuItem &)> handleActivate = defaultHandleActivate;

  std::unique_ptr<Menu> subMenu;
};

struct Menu {
  Output<vec2> position;
  AngularParticle rotation;

  std::vector<std::unique_ptr<MenuItem>> items;
  MenuItem *activeItem = nullptr;

  float indexedRotation;
  size_t currentIndex;

  float tileRadius = screenWidth * 0.5f;

  std::chrono::steady_clock::time_point lastCrankTime;

  MenuItem *makeItem() {
    items.emplace_back(new MenuItem());
    return items.back().get();
  }

  void turn(float amount) {
    rotation.angle += amount / items.size();
    lastCrankTime = std::chrono::steady_clock::now();
  }

  void step() {
    rotation.friction = activeItem ? 0.2f : 0.1f;
    rotation.step();

    indexedRotation = rotation.angle / TWO_PI * items.size();
    currentIndex = std::fmod(std::round(indexedRotation), items.size());

    auto timeSinceLastCrank = std::chrono::steady_clock::now() - lastCrankTime;
    if (timeSinceLastCrank > std::chrono::milliseconds(350)) {
      if (!activeItem) {
        activeItem = items[currentIndex].get();
        if (activeItem->handleSelect) activeItem->handleSelect(*activeItem);
      }
      rotation.lerp(float(currentIndex) / items.size() * TWO_PI, 0.2f);
    }
  }

  void draw() const {
    auto radius = regularPolyRadius(tileRadius * 2.2f, items.size());
    auto angleIncr = -TWO_PI / items.size();

    pushTransform();
    translate(position() + vec2(radius, 0.0f));
    rotate(rotation.angle);

    auto drawItem = [&](size_t i) {
      const auto &item = *items[i];
      pushTransform();
      rotate(float(i) / items.size() * -TWO_PI);
      translate(-radius, 0.0f);
      scale(item.scale());
      item.handleDraw(item);
      popTransform();
    };

    drawItem(currentIndex);

    float offset = indexedRotation - currentIndex;
    if (offset < -0.25f) drawItem((items.size() + currentIndex - 1) % items.size());
    if (offset > 0.25f) drawItem((currentIndex + 1) % items.size());

    popTransform();
  }
};

struct MenuMode {
  std::unique_ptr<Menu> rootMenu = std::make_unique<Menu>();

  std::vector<Menu *> menuStack;
  Menu *activeMenu = nullptr;
  Menu *deactivatingMenu = nullptr;

  float secondsPerFrame;
  uint32_t frameCount = 0;
};

static MenuMode mode;

static void activateMenu(Menu *menu, bool pushToStack) {
  // NOTE(ryan): Bail if there's already an activation in progress. We do this here to make the
  // user-facing API less error prone.
  if (mode.deactivatingMenu) return;

  float direction = pushToStack ? 1.0f : -1.0f;

  // Deactivate previously active menu and animate out
  {
    mode.deactivatingMenu = mode.activeMenu;

    timeline.apply(&mode.deactivatingMenu->position)
        .then<RampTo>(vec2(-screenWidth * direction, 0.0f), 0.3f, EaseInOutQuad())
        .finishFn([&](Motion<vec2> &m) { mode.deactivatingMenu = nullptr; });

    if (pushToStack) {
      mode.menuStack.push_back(mode.deactivatingMenu);
    }
  }

  // Animate in the new active menu
  menu->position = vec2(screenWidth * direction, 0.0f);
  timeline.apply(&menu->position).then<RampTo>(vec2(), 0.3f, EaseInOutQuad());
  mode.activeMenu = menu;
}

static void activatePreviousMenu() {
  if (!mode.deactivatingMenu && mode.menuStack.size()) {
    activateMenu(mode.menuStack.back(), false);
    mode.menuStack.pop_back();
  }
}

static void fillTextFitToWidth(const std::string &text, float width, float height) {
  fontSize(1.0f);
  auto size = getTextBounds(text).size;
  fontSize(std::min(width / size.x, height / size.y));
  fillText(text);
}


STAK_EXPORT int init() {
  loadFont("assets/232MKSD-round-light.ttf");

  mode.activeMenu = mode.rootMenu.get();

  auto makeTextDraw =
      [](const std::string &text, float width = screenWidth * 0.6f, float height = 40.0f) {
    return [=](const MenuItem &item) {
      MenuItem::defaultHandleDraw(item);
      textAlign(ALIGN_MIDDLE | ALIGN_CENTER);
      fillColor(0.0f, 0.0f, 0.0f);
      fillTextFitToWidth(text, width, height);
    };
  };

  {
    auto modes = mode.rootMenu->makeItem();
    modes->handleDraw = makeTextDraw(".gif");
  }

  {
    auto wifi = mode.rootMenu->makeItem();
    wifi->handleDraw = makeTextDraw("wifi");
    wifi->subMenu = std::make_unique<Menu>();
    wifi->subMenu->makeItem()->handleDraw = makeTextDraw("on/off");
    auto ex = wifi->subMenu->makeItem();
    ex->handleDraw = makeTextDraw("X");
    ex->handleActivate = [](MenuItem &item) { activatePreviousMenu(); };
  }

  {
    auto item = mode.rootMenu->makeItem();
    item->handleDraw = makeTextDraw("98%");
  }

  {
    auto item = mode.rootMenu->makeItem();
    item->handleDraw = makeTextDraw("512MB");
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
  mode.activeMenu->step();

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

  if (mode.deactivatingMenu) mode.deactivatingMenu->draw();
  mode.activeMenu->draw();

  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  auto &menu = *mode.activeMenu;

  menu.turn(amount * -0.05f);

  if (menu.activeItem) {
    if (menu.activeItem->handleDeselect) menu.activeItem->handleDeselect(*menu.activeItem);
    menu.activeItem = nullptr;
  }

  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  auto activeItem = mode.activeMenu->activeItem;
  if (activeItem && activeItem->handlePress) activeItem->handlePress(*activeItem);

  return 0;
}

STAK_EXPORT int shutter_button_released() {
  // stak_activate_mode();

  auto activeItem = mode.activeMenu->activeItem;
  if (activeItem) {
    if (activeItem->handleRelease) activeItem->handleRelease(*activeItem);
    if (activeItem->handleActivate) activeItem->handleActivate(*activeItem);
  }

  return 0;
}

STAK_EXPORT int power_button_pressed() {
  activatePreviousMenu();
  return 0;
}

STAK_EXPORT int power_button_released() {
  return 0;
}

STAK_EXPORT int crank_pressed() {
  return 0;
}

STAK_EXPORT int crank_released() {
  return 0;
}
