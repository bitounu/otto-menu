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

  Svg *iconBack, *iconBattery, *iconHdd, *iconNo, *iconWifi, *iconYes;

  float secondsPerFrame;
  uint32_t frameCount = 0;
};

static MenuMode mode;

struct Toggle {
  bool enabled;

  Toggle(bool enabled = false) : enabled{ enabled } {}
};

static void fillTextFitToWidth(const std::string &text, float width, float height) {
  fontSize(1.0f);
  auto size = getTextBounds(text).size;
  fontSize(std::min(width / size.x, height / size.y));
  fillText(text);
}

STAK_EXPORT int init() {
  loadFont("assets/232MKSD-RoundMedium.ttf");

  // Load images
  mode.iconBack    = loadSvg("assets/icon-back.svg", "px", 96);
  mode.iconBattery = loadSvg("assets/icon-battery.svg", "px", 96);
  mode.iconHdd     = loadSvg("assets/icon-hdd.svg", "px", 96);
  mode.iconNo      = loadSvg("assets/icon-no.svg", "px", 96);
  mode.iconWifi    = loadSvg("assets/icon-wifi.svg", "px", 96);
  mode.iconYes     = loadSvg("assets/icon-yes.svg", "px", 96);

  mode.rootMenu = makeMenu(mode.entities);

  auto menus = mode.systems.add<MenuSystem>(vec2(screenWidth, screenHeight));
  menus->activateMenu(mode.rootMenu);

  mode.systems.configure();

  auto makeTextDraw =
      [](const std::string &text, float width = screenWidth * 0.6f, float height = 40.0f) {
    return [=](Entity e) {
      MenuItem::defaultHandleDraw(e);
      textAlign(ALIGN_MIDDLE | ALIGN_CENTER);
      fillColor(1.0f, 1.0f, 1.0f);
      fillTextFitToWidth(text, width, height);
    };
  };

  auto makeIconDraw = [](Svg *svg) {
    return [svg](Entity e) {
      MenuItem::defaultHandleDraw(e);
      translate(vec2(-48.0f));
      draw(svg);
    };
  };

  auto assignPressHoldLabel = [](Entity e, const std::string &text) {
    e.replace<PressHandler>([=](MenuSystem &ms, Entity e) {
      // MenuItem::defaultHandlePress(ms, e);
      ms.displayLabelInfinite(text);
    });
    e.replace<ReleaseHandler>([](MenuSystem &ms, Entity e) {
      // MenuItem::defaultHandleRelease(ms, e);
      ms.hideLabel();
    });
  };

  {
    auto modes = makeMenuItem(mode.entities, mode.rootMenu);
    modes.replace<DrawHandler>(makeTextDraw("gif"));
    assignPressHoldLabel(modes, "No gif :(");
  }

  {
    auto itemEntity = makeMenuItem(mode.entities, mode.rootMenu);
    itemEntity.assign<Label>("wifi");
    itemEntity.replace<DrawHandler>(makeIconDraw(mode.iconWifi));

    auto item = itemEntity.component<MenuItem>();
    item->subMenu = makeMenu(mode.entities);

    auto tog = makeMenuItem(mode.entities, item->subMenu);
    tog.replace<DrawHandler>([](Entity e) {
      MenuItem::defaultHandleDraw(e);
      translate(vec2(-48.0f));
      draw(e.component<Toggle>()->enabled ? mode.iconYes : mode.iconNo);
    });
    tog.replace<ActivateHandler>([](MenuSystem &ms, Entity e) {
      timeline.apply(&e.component<Scale>()->scale)
          .then<RampTo>(vec2(0.0f), 0.1f, EaseInQuad())
          .then<RampTo>(vec2(1.0f), 0.1f, EaseOutQuad())
          .then<Hold>(vec2(1.0f), 0.15f)
          .finishFn([e, &ms](ch::Motion<vec2> &m) mutable {
            ms.displayLabel(e.component<Label>()->getLabel(e));
          });
      timeline.cue([e, &ms]() mutable {
        auto toggle = e.component<Toggle>();
        toggle->enabled = !toggle->enabled;
      }, 0.1f);
      ms.hideLabel();
    });
    tog.assign<Toggle>(false);
    tog.assign<Label>([](Entity e) {
      return e.component<Toggle>()->enabled ? "wifi on" : "wifi off";
    });

    auto back = makeMenuItem(mode.entities, item->subMenu);
    back.assign<Label>("back");
    back.replace<DrawHandler>(makeIconDraw(mode.iconBack));
    back.replace<ActivateHandler>([](MenuSystem &ms, Entity e) {
      ms.activatePreviousMenu();
    });
  }

  {
    auto item = makeMenuItem(mode.entities, mode.rootMenu);
    item.assign<Label>("battery");
    item.replace<DrawHandler>(makeIconDraw(mode.iconBattery));
    assignPressHoldLabel(item, "85%");
  }

  {
    auto item = makeMenuItem(mode.entities, mode.rootMenu);
    item.assign<Label>("memory");
    item.replace<DrawHandler>(makeIconDraw(mode.iconHdd));
    assignPressHoldLabel(item, "128MiB/4GiB");
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
