#include "stak.h"
#include "gfx.hpp"
#include "util.hpp"
#include "menu.hpp"
#include "rand.hpp"
#include "fx.hpp"

#include "gtx/string_cast.hpp"
#include "entityx/entityx.h"

#include <chrono>

// Debug
#include <iostream>

using namespace glm;
using namespace choreograph;
using namespace otto;

static const int screenWidth = 96, screenHeight = 96;
static const vec2 screenSize = { screenWidth, screenHeight };
static const Rect screenBounds = { vec2(), screenSize };

static const float displayDaydreamDelay = 15.0f;
static const float displaySleepDelay = 30.0f;

static struct MenuMode : public entityx::EntityX {
  Entity rootMenu;

  Svg *iconBatteryMask, *iconMemoryMask;

  double time = 0.0;

  float secondsPerFrame;
  uint32_t frameCount = 0;

  ch::TimelineItemControlRef displaySleepTimeout;
  ch::Output<float> displayBrightness = 1.0f;
  bool displayIsSleeping = false;
} mode;

struct Toggle {
  bool enabled;

  Toggle(bool enabled = false) : enabled{ enabled } {}
};

struct DetailView {
  Output<float> generalScale = 1.0f;
  Output<float> detailScale = 0.0f;

  std::chrono::steady_clock::time_point pressTime;
  bool isPressed = false;

  bool okToRelease() {
    auto now = std::chrono::steady_clock::now();
    return now - pressTime > std::chrono::seconds(1);
  }

  void press() {
    if (detailScale > 0.0f) return;

    timeline.apply(&generalScale).then<RampTo>(0.0f, 0.15f, EaseInQuad());
    timeline.apply(&detailScale).then<Hold>(0.0f, 0.15f).then<RampTo>(1.0f, 0.15f, EaseOutQuad());
    timeline.cue([this] { if (!isPressed) release(); }, 1.1f).getControl();

    isPressed = true;
    pressTime = std::chrono::steady_clock::now();
  }

  void release() {
    if (okToRelease()) {
      timeline.apply(&detailScale).then<RampTo>(0.0f, 0.15f, EaseOutQuad());
      timeline.apply(&generalScale)
          .then<Hold>(0.0f, generalScale == 0.0f ? 0.15f : 0.0f)
          .then<RampTo>(1.0f, 0.15f, EaseOutQuad());
    }
    isPressed = false;
  }
};


static void sleepDisplay() {
  timeline.apply(&mode.displayBrightness)
      .then<RampTo>(0.5f, 2.0f, EaseInOutQuad())
      .then<Hold>(0.5f, displaySleepDelay - displayDaydreamDelay)
      .then<RampTo>(0.0f, 2.0f, EaseInQuad())
      .finishFn([](ch::Motion<float> &m) { mode.displayIsSleeping = true; });
}

static bool wakeDisplay() {
  if (mode.displaySleepTimeout) {
    mode.displaySleepTimeout->cancel();
    mode.displaySleepTimeout.reset();
  }

  timeline.apply(&mode.displayBrightness).then<RampTo>(1.0f, 0.25f, EaseOutQuad());
  mode.displaySleepTimeout =
      timeline.cue([] { sleepDisplay(); }, displayDaydreamDelay).getControl();

  auto displayWasSleeping = mode.displayIsSleeping;
  mode.displayIsSleeping = false;
  return displayWasSleeping;
}


static void fillTextCenteredWithSuffix(const std::string &text, const std::string &suffix,
                                       float textSize, float suffixSize) {
  ScopedTransform xf;

  fontSize(textSize);
  auto textBounds = getTextBounds(text);
  fontSize(suffixSize);
  auto suffixBounds = getTextBounds(suffix);

  textAlign(ALIGN_LEFT | ALIGN_BASELINE);
  translate(-0.5f * (textBounds.size.x + suffixBounds.size.x), 0);
  fontSize(textSize);
  fillText(text);
  translate(textBounds.size.x, 0);
  fontSize(suffixSize);
  fillText(suffix);
}

STAK_EXPORT int init() {
  auto assets = std::string(stak_assets_path());

  loadFont(assets + "232MKSD-round-medium.ttf");

  // Load images
  mode.iconBatteryMask = loadSvg(assets + "icon-battery-mask.svg", "px", 96);
  mode.iconMemoryMask = loadSvg(assets + "icon-memory-mask.svg", "px", 96);

  mode.rootMenu = makeMenu(mode.entities);

  auto menus = mode.systems.add<MenuSystem>(screenSize);
  menus->activateMenu(mode.rootMenu);

  mode.systems.configure();

  auto fillTextFitToWidth = [](const std::string &text, float width, float height) {
    fontSize(1.0f);
    auto size = getTextBounds(text).size;
    fontSize(std::min(width / size.x, height / size.y));
    fillText(text);
  };

  auto makeTextDraw = [=](const std::string &text, float width = 50.0f, float height = 40.0f) {
    return [=](Entity e) {
      MenuItem::defaultHandleDraw(e);
      textAlign(ALIGN_MIDDLE | ALIGN_CENTER);
      fillColor(1.0f, 1.0f, 1.0f);
      fillTextFitToWidth(text, width, height);
    };
  };

  auto assignDetailPressHandlers = [](Entity e) {
    e.replace<PressHandler>(
        [](MenuSystem &ms, Entity e) { e.component<DetailView>()->press(); });
    e.replace<ReleaseHandler>(
        [](MenuSystem &ms, Entity e) { e.component<DetailView>()->release(); });
  };

  //
  // GIF Mode
  //
  {
    auto gif = makeMenuItem(mode.entities, mode.rootMenu);
    gif.replace<DrawHandler>(makeTextDraw("gif"));
    gif.replace<ActivateHandler>([](MenuSystem &ms, Entity e) { stak_activate_mode(); });
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
      fontSize(18);
      textAlign(ALIGN_CENTER | ALIGN_BASELINE);
      fillColor(vec3(1));
      fillText(e.component<Toggle>()->enabled ? "OTTO" : "OFF");
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
    auto bat = makeMenuItem(mode.entities, mode.rootMenu);
    bat.assign<Label>("battery");
    bat.assign<DetailView>();
    assignDetailPressHandlers(bat);
    bat.replace<DrawHandler>([](Entity e) {
      auto detail = e.component<DetailView>();

      if (detail->generalScale > 0.0f) {
        scale(detail->generalScale);

        ScopedMask mask(screenSize);
        {
          ScopedTransform xf;
          translate(screenSize * -0.5f);

          beginMask();
          draw(mode.iconBatteryMask);
          endMask();

          beginPath();
          rect(screenBounds);
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
      }

      if (detail->detailScale > 0.0f) {
        scale(detail->detailScale);

        fillColor(vec3(1));

        pushTransform();
        translate(0, 8);
        textAlign(ALIGN_CENTER | ALIGN_BASELINE);
        fontSize(20);
        fillText("55%");
        popTransform();

        beginPath();
        moveTo(-20, 0);
        lineTo(20, 0);
        strokeCap(VG_CAP_SQUARE);
        strokeWidth(2);
        strokeColor(vec3(0.35f));
        stroke();

        pushTransform();
        translate(0, -23);
        fillTextCenteredWithSuffix("1.2", "hrs", 21, 14);
        popTransform();
      }
    });
  }

  //
  // Memory
  //
  {
    auto drawBytes = [](uint32_t bytes) {
      auto mb = formatMebibytes(bytes);
      fillTextCenteredWithSuffix(mb.first, mb.second, 21, 14);
    };

    auto mem = makeMenuItem(mode.entities, mode.rootMenu);
    mem.assign<Label>("memory");
    mem.assign<Bubbles>(Rect(15, 18, 65, 56), 8.0f);
    mem.assign<DetailView>();
    assignDetailPressHandlers(mem);
    mem.replace<DrawHandler>([=](Entity e) {
      auto detail = e.component<DetailView>();

      if (detail->generalScale > 0.0f) {
        scale(detail->generalScale);

        ScopedMask mask(screenSize);
        translate(screenSize * -0.5f);

        beginMask();
        draw(mode.iconMemoryMask);
        endMask();

        beginPath();
        rect(screenBounds);
        fillColor(vec3(0.35f));
        fill();

        e.component<Bubbles>()->draw();
      }

      if (detail->detailScale > 0.0f) {
        scale(detail->detailScale);

        fillColor(vec3(1));

        pushTransform();
        translate(0, 8);
        drawBytes(531006081);
        popTransform();

        beginPath();
        moveTo(-20, 0);
        lineTo(20, 0);
        strokeCap(VG_CAP_SQUARE);
        strokeWidth(2);
        strokeColor(vec3(0.35f));
        stroke();

        pushTransform();
        translate(0, -23);
        drawBytes(1073741824);
        popTransform();
      }
    });
  }

  wakeDisplay();

  return 0;
}

STAK_EXPORT int shutdown() {
  return 0;
}

STAK_EXPORT int update(float dt) {
  if (mode.displayIsSleeping) return 0;

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

  if (mode.displayIsSleeping) return 0;

  clearColor(vec3(0.0f));
  clear(screenBounds);

  setTransform(defaultMatrix);

  // NOTE(ryan): Apply a circular mask to simulate a round display. We may want to move this to
  // stak-sdk so that the mask is enforced.
  fillMask(screenBounds);
  beginPath();
  circle(48.0f, 48.0f, 48.0f);
  beginMask();
  fill();
  endMask();
  enableMask();

  enableColorTransform();
  setColorTransform(vec4(vec3(mode.displayBrightness()), 1.0f), vec4(0.0f));

  ScopedMask mask(screenSize);
  mode.systems.system<MenuSystem>()->draw();

  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  mode.systems.system<MenuSystem>()->turn(amount * -0.25f);
  wakeDisplay();
  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  if (!wakeDisplay()) mode.systems.system<MenuSystem>()->pressItem();
  return 0;
}

STAK_EXPORT int shutter_button_released() {
  auto ms = mode.systems.system<MenuSystem>();
  ms->releaseAndActivateItem();
  wakeDisplay();
  return 0;
}

STAK_EXPORT int power_button_pressed() {
  mode.systems.system<MenuSystem>()->indicatePreviousMenu();
  wakeDisplay();
  return 0;
}

STAK_EXPORT int power_button_released() {
  mode.systems.system<MenuSystem>()->activatePreviousMenu();
  wakeDisplay();
  return 0;
}

STAK_EXPORT int crank_pressed() {
  wakeDisplay();
  return 0;
}

STAK_EXPORT int crank_released() {
  wakeDisplay();
  return 0;
}
