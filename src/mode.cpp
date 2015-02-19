#include "stak.h"
#include "gfx.hpp"
#include "util.hpp"
#include "menu.hpp"

#include "entityx/entityx.h"

// Debug
#include <iostream>

#define STAK_EXPORT extern "C"

using namespace glm;
using namespace choreograph;
using namespace otto;

static const float screenWidth = 96.0f;
static const float screenHeight = 96.0f;

struct MenuMode : public entityx::EntityX {
  Entity rootMenu;

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

  mode.rootMenu = makeMenu(mode.entities);

  auto menus = mode.systems.add<MenuSystem>(vec2(screenWidth, screenHeight));
  menus->activateMenu(mode.rootMenu);

  mode.systems.configure();

  auto makeTextDraw =
      [](const std::string &text, float width = screenWidth * 0.6f, float height = 40.0f) {
    return [=](const Entity e) {
      MenuItem::defaultHandleDraw(e);
      textAlign(ALIGN_MIDDLE | ALIGN_CENTER);
      fillColor(0.0f, 0.0f, 0.0f);
      fillTextFitToWidth(text, width, height);
    };
  };

  {
    auto modes = makeMenuItem(mode.entities, mode.rootMenu);
    modes.replace<DrawHandler>(makeTextDraw(".gif"));
  }

  {
    auto e = makeMenuItem(mode.entities, mode.rootMenu);
    e.replace<DrawHandler>(makeTextDraw("wifi"));
    auto wifi = e.component<MenuItem>();
    wifi->subMenu = makeMenu(mode.entities);
    makeMenuItem(mode.entities, wifi->subMenu).replace<DrawHandler>(makeTextDraw("on/off"));
    auto ex = makeMenuItem(mode.entities, wifi->subMenu);
    ex.replace<DrawHandler>(makeTextDraw("X"));
    ex.replace<ActivateHandler>([](MenuSystem &ms, Entity e) { ms.activatePreviousMenu(); });
  }

  {
    auto item = makeMenuItem(mode.entities, mode.rootMenu);
    item.replace<DrawHandler>(makeTextDraw("98%"));
  }

  {
    auto item = makeMenuItem(mode.entities, mode.rootMenu);
    item.replace<DrawHandler>(makeTextDraw("512MB"));
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
  mode.systems.update<MenuSystem>(dt);

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

  mode.systems.system<MenuSystem>()->draw();

  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  mode.systems.system<MenuSystem>()->turn(amount * -0.2f);
  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  mode.systems.system<MenuSystem>()->pressItem();
  return 0;
}

STAK_EXPORT int shutter_button_released() {
  auto ms = mode.systems.system<MenuSystem>();
  ms->releaseItem();
  ms->activateItem();
  return 0;
}

STAK_EXPORT int power_button_pressed() {
  mode.systems.system<MenuSystem>()->activatePreviousMenu();
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
