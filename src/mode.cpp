#include "stak.h"
#include "gfx.hpp"
#include "util.hpp"
#include "menu.hpp"

// Debug
#include <iostream>

#define STAK_EXPORT extern "C"

using namespace glm;
using namespace choreograph;
using namespace otto;

static const float screenWidth = 96.0f;
static const float screenHeight = 96.0f;

struct MenuMode {
  std::unique_ptr<Menu> rootMenu;

  MenuSystem menus = { vec2(screenWidth) };

  float secondsPerFrame;
  uint32_t frameCount = 0;
};

static MenuMode mode;

static void fillTextFitToWidth(const std::string &text, float width, float height) {
  fontSize(1.0f);
  auto size = getTextBounds(text).size;
  fontSize(std::min(width / size.x, height / size.y));
  fillText(text);
}

STAK_EXPORT int init() {
  loadFont("assets/232MKSD-round-light.ttf");

  mode.rootMenu = std::make_unique<Menu>();
  mode.menus.activateMenu(mode.rootMenu.get());

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
    ex->handleActivate = [](MenuSystem &ms, MenuItem &item) { ms.activatePreviousMenu(); };
  }

  {
    auto item = mode.rootMenu->makeItem();
    item->handleDraw = makeTextDraw("98%");
  }

  {
    auto item = mode.rootMenu->makeItem();
    item->handleDraw = makeTextDraw("512MB");
  }

  // for (auto &item : mode.rootMenu->items) {
  //   if (item->handleDeselect) item->handleDeselect(*item);
  // }

  return 0;
}

STAK_EXPORT int shutdown() {
  return 0;
}

STAK_EXPORT int update(float dt) {
  timeline.step(dt);
  mode.menus.step();

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

  mode.menus.draw();

  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  mode.menus.turn(amount * -0.2f);
  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  mode.menus.pressItem();
  return 0;
}

STAK_EXPORT int shutter_button_released() {
  mode.menus.releaseItem();
  mode.menus.activateItem();
  return 0;
}

STAK_EXPORT int power_button_pressed() {
  mode.menus.activatePreviousMenu();
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
