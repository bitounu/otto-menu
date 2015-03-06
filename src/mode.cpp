#include "stak.h"
#include "gfx.hpp"
#include "util.hpp"
#include "menu.hpp"
#include "rand.hpp"

#include "gtx/string_cast.hpp"
#include "entityx/entityx.h"

#include <algorithm>

// Debug
#include <iostream>

// TODO(ryan): Move this to stak.h
#define STAK_EXPORT extern "C"

using namespace glm;
using namespace choreograph;
using namespace otto;

static const int screenWidth = 96, screenHeight = 96;
static const vec2 screenSize = { screenWidth, screenHeight };

struct MenuMode : public entityx::EntityX {
  Entity rootMenu;

  Svg *iconBack, *iconBatteryMask, *iconMemoryMask, *iconNo, *iconWifi, *iconYes;

  double time = 0.0;

  float secondsPerFrame;
  uint32_t frameCount = 0;
};

static MenuMode mode;

struct Toggle {
  bool enabled;

  Toggle(bool enabled = false) : enabled{ enabled } {}
};

struct Bubble {
  vec2 position;
  vec3 color;
  ch::Output<float> scale;
};

struct Bubbles {
  std::vector<Bubble> bubbles;
  std::vector<vec3> colors = { colorBGR(0x00ADEF), colorBGR(0xEC008B), colorBGR(0xFFF100) };

  Rect bounds;
  float bubbleRadius;
  size_t bubbleCount = 0;

  VGPath circlePath;

  Bubbles(const Rect &bounds, float bubbleRadius)
  : bounds(bounds),
    bubbleRadius{ bubbleRadius },
    circlePath{ vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0,
                             VG_PATH_CAPABILITY_ALL) } {
    size_t maxBubbles = bounds.getArea() / (M_PI * (bubbleRadius * bubbleRadius)) * 2.0f;
    bubbles.resize(maxBubbles);

    circle(circlePath, 0, 0, bubbleRadius);
    setCount(bubbles.size());
  }
  ~Bubbles() {
    vgDestroyPath(circlePath);
  }

  void startBubbleAnim(size_t i, bool delay) {
    auto &bubble = bubbles[i];
    bubble.position = randVec2(bounds);
    bubble.color = colors[randInt(colors.size())];
    timeline.apply(&bubble.scale)
        .then<Hold>(0.0f, delay ? float(i) / bubbleCount * 2.0f : 0.0f)
        .then<RampTo>(1.0f, 1.0f, EaseOutQuad())
        .then<RampTo>(0.0f, 1.0f, EaseInQuad())
        .finishFn([=](ch::Motion<float> &m) { startBubbleAnim(i, false); });
  }

  void stopBubbleAnim(size_t i) {
    timeline.apply(&bubbles[i].scale).then<RampTo>(0.0f, 1.0f, EaseOutQuad());
  }

  void setCount(size_t count) {
    bubbleCount = count;
    for (size_t i = 0; i < bubbles.size(); ++i) {
      if (i < count) startBubbleAnim(i, true);
      else stopBubbleAnim(i);
    }
  }

  void draw() {
    for (size_t i = 0; i < bubbleCount; ++i) {
      const auto &bubble = bubbles[i];
      ScopedTransform xf;
      translate(bubble.position);
      scale(bubble.scale());
      fillColor(bubble.color);
      vgDrawPath(circlePath, VG_FILL_PATH);
    }
  }
};

struct Blip {
  Output<vec3> color;
  Output<float> scale = 0.0f;

  void draw() {
    beginPath();
    circle(0, 0, scale());
    fillColor(color());
    fill();
  }
};

struct Blips {
  std::vector<std::unique_ptr<Blip>> blips;
  Blip centerBlip;

  std::vector<vec3> colors = { colorBGR(0x00ADEF), colorBGR(0xEC008B), colorBGR(0xFFF100) };

  size_t nextColorIndex = 0;
  float blipRadius = 40.0f;
  bool animating = false;

  Blips() {
    for (size_t i = 0; i < 2; ++i) {
      blips.push_back(std::make_unique<Blip>());
    }
    centerBlip.color = vec3(0.35f);
    centerBlip.scale = 7.0f;
  }

  void startBlipAnim(Blip &blip, float delay) {
    const auto color = colors[nextColorIndex];

    timeline.apply(&blip.scale)
        .then<Hold>(7.0f, delay)
        .then<RampTo>(blipRadius, 2.0f, EaseOutQuad())
        .then<Hold>(0.0f, 0.0f);
    timeline.apply(&blip.color)
        .then<Hold>(color, delay + 1.0f)
        .then<RampTo>(vec3(), 1.0f, EaseOutQuad())
        .finishFn([this, &blip](ch::Motion<vec3> &m) {
          std::rotate(blips.begin(), blips.begin() + 1, blips.end());
          if (animating) startBlipAnim(blip, 0.0f);
        });

    timeline.apply(&centerBlip.color)
        .then<Hold>(glm::mix(color, vec3(1), 0.75f), 0.0f)
        .then<RampTo>(vec3(0.35f), 0.5f, EaseInQuad());

    nextColorIndex = (nextColorIndex + 1) % colors.size();
  }

  void startAnim() {
    animating = true;
    for (size_t i = 0; i < blips.size(); ++i) {
      startBlipAnim(*blips[i], float(i) / blips.size() * 2.0f);
    }
  }

  void stopAnim() { animating = false; }

  void draw() {
    for (const auto &blip : blips) {
      blip->draw();
    }
  }
  void drawCenter() { centerBlip.draw(); }
};

static void fillTextFitToWidth(const std::string &text, float width, float height) {
  fontSize(1.0f);
  auto size = getTextBounds(text).size;
  fontSize(std::min(width / size.x, height / size.y));
  fillText(text);
}

STAK_EXPORT int init() {
  auto assets = std::string(stak_assets_path());

  loadFont(assets + "232MKSD-round-medium.ttf");

  // Load images
  mode.iconBack        = loadSvg(assets + "icon-back.svg", "px", 96);
  mode.iconBatteryMask = loadSvg(assets + "icon-battery-mask.svg", "px", 96);
  mode.iconMemoryMask  = loadSvg(assets + "icon-memory-mask.svg", "px", 96);
  mode.iconNo          = loadSvg(assets + "icon-no.svg", "px", 96);
  mode.iconWifi        = loadSvg(assets + "icon-wifi.svg", "px", 96);
  mode.iconYes         = loadSvg(assets + "icon-yes.svg", "px", 96);

  mode.rootMenu = makeMenu(mode.entities);

  auto menus = mode.systems.add<MenuSystem>(screenSize);
  menus->activateMenu(mode.rootMenu);

  mode.systems.configure();

  auto makeTextDraw = [](const std::string &text, float width = 50.0f, float height = 40.0f) {
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
    e.replace<PressHandler>([=](MenuSystem &ms, Entity e) { ms.displayLabelInfinite(text); });
    e.replace<ReleaseHandler>([](MenuSystem &ms, Entity e) { ms.hideLabel(); });
  };

  //
  // GIF Mode
  //
  {
    auto gif = makeMenuItem(mode.entities, mode.rootMenu);
    gif.replace<DrawHandler>(makeTextDraw("gif"));
    gif.replace<ActivateHandler>([](MenuSystem &ms, Entity e) {
      stak_activate_mode();
    });
  }

  //
  // Wifi
  //
  {
    auto wifi = makeMenuItem(mode.entities, mode.rootMenu);
    wifi.assign<Label>("wifi");
    wifi.assign<Toggle>();
    wifi.assign<Blips>();
    wifi.replace<DrawHandler>([](Entity e) {
      {
        ScopedTransform xf;
        translate(0, 20);

        e.component<Blips>()->draw();

        beginPath();
        moveTo(0, 0);
        lineTo(0, -25);
        strokeWidth(4);
        strokeCap(VG_CAP_ROUND);
        strokeColor(vec3(0.35f));
        stroke();

        e.component<Blips>()->drawCenter();
      }

      translate(0, -30);
      fontSize(13);
      textAlign(ALIGN_CENTER | ALIGN_BASELINE);
      fillColor(vec3(1));
      fillText(e.component<Toggle>()->enabled ? "ON" : "OFF");
    });
    wifi.replace<ActivateHandler>([](MenuSystem &ms, Entity e) {
      auto toggle = e.component<Toggle>();
      if (toggle->enabled = !toggle->enabled) {
        e.component<Blips>()->startAnim();
        ms.displayLabel("wifi on");
      } else {
        e.component<Blips>()->stopAnim();
        ms.displayLabel("wifi off");
      }
    });
  }

  //
  // Battery
  //
  {
    auto item = makeMenuItem(mode.entities, mode.rootMenu);
    item.assign<Label>("battery");
    item.replace<DrawHandler>([](Entity e) {
      ScopedMask mask(screenSize);
      {
        ScopedTransform xf;
        translate(screenSize * -0.5f);

        beginMask();
        draw(mode.iconBatteryMask);
        endMask();

        beginPath();
        rect(vec2(), screenSize);
        fillColor(vec3(0.35f));
        fill();
      }
      beginPath();
      float t = mode.time * 2.0f;
      moveTo(-48.0f, std::sin(t) / M_PI * 40.0f);
      lineTo(48.0f, std::sin(t + 0.5f) / M_PI * 40.0f);
      lineTo(48, -48);
      lineTo(-48, -48);
      fillColor(0, 1, 0);
      fill();
    });
    assignPressHoldLabel(item, "99%");
  }

  //
  // Memory
  //
  {
    auto item = makeMenuItem(mode.entities, mode.rootMenu);
    item.assign<Label>("memory");
    item.assign<Bubbles>(Rect(15, 18, 65, 56), 8.0f);
    item.replace<DrawHandler>([](Entity e) {
      ScopedMask mask(screenSize);
      translate(screenSize * -0.5f);

      beginMask();
      draw(mode.iconMemoryMask);
      endMask();

      beginPath();
      rect(vec2(), screenSize);
      fillColor(vec3(0.35f));
      fill();

      e.component<Bubbles>()->draw();
    });
    assignPressHoldLabel(item, "128MiB/4GiB");
  }

  return 0;
}

STAK_EXPORT int shutdown() {
  return 0;
}

STAK_EXPORT int update(float dt) {
  mode.time += dt;

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
  static const mat3 defaultMatrix = { 0.0, -1.0, 0.0, 1.0, -0.0, 0.0, 0.0, screenHeight, 1.0 };

  clearColor(0, 0, 0);
  clear(0, 0, 96, 96);

  setTransform(defaultMatrix);

  // NOTE(ryan): Apply a circular mask to simulate a round display. We may want to move this to
  // stak-sdk so that the mask is enforced.
  fillMask(vec2(), screenSize);
  beginPath();
  circle(48.0f, 48.0f, 48.0f);
  beginMask();
  fill();
  endMask();
  enableMask();

  ScopedMask mask(screenSize);
  mode.systems.system<MenuSystem>()->draw();

  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  mode.systems.system<MenuSystem>()->turn(amount * -0.25f);
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
  mode.systems.system<MenuSystem>()->indicatePreviousMenu();
  return 0;
}

STAK_EXPORT int power_button_released() {
  mode.systems.system<MenuSystem>()->activatePreviousMenu();
  return 0;
}

STAK_EXPORT int crank_pressed() {
  return 0;
}

STAK_EXPORT int crank_released() {
  return 0;
}
