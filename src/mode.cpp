#include "stak.h"
#include "stak/devices/disk.hpp"
#include "stak/devices/power.hpp"
#include "stak/devices/wifi.hpp"

#include "display.hpp"
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

static const float detailDurationMin = 1.0f;

static struct MenuMode : public entityx::EntityX {
  Entity rootMenu;

  Svg *iconBatteryMask, *iconMemoryMask, *iconCharging;

  double time = 0.0;

  float secondsPerFrame;
  uint32_t frameCount = 0;
} mode;

static Display display = { { 96.0f, 96.0f } };

struct DiskSpace {
  uint64_t used, total;
};

struct Power {
  float percentCharged;
  bool isCharging;
};

struct DetailView {
  Output<float> generalScale = 1.0f;
  Output<float> detailScale = 0.0f;

  std::chrono::steady_clock::time_point pressTime;
  bool isPressed = false;

  bool okToRelease() {
    auto now = std::chrono::steady_clock::now();
    return now - pressTime >
           std::chrono::milliseconds(static_cast<int>(950.0f * detailDurationMin));
  }

  void press() {
    isPressed = true;

    if (detailScale > 0.0f) return;

    timeline.apply(&generalScale).then<RampTo>(0.0f, 0.15f, EaseInQuad());
    timeline.apply(&detailScale).then<Hold>(0.0f, 0.15f).then<RampTo>(1.0f, 0.15f, EaseOutQuad());
    timeline.cue([this] { if (!isPressed) release(); }, detailDurationMin);

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

  stakWifiSetSsid("OTTO");

  // Load images
  mode.iconBatteryMask = loadSvg(assets + "icon-battery-mask.svg", "px", 96);
  mode.iconMemoryMask = loadSvg(assets + "icon-memory-mask.svg", "px", 96);
  mode.iconCharging = loadSvg(assets + "icon-charging.svg", "px", 96);

  mode.rootMenu = makeMenu(mode.entities);

  auto menus = mode.systems.add<MenuSystem>(display.bounds.size);
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
      fillText(stakWifiIsEnabled() ? stakWifiSsid() : "OFF");
    });
    wifi.replace<ActivateHandler>([](MenuSystem &ms, Entity e) {
      if (!stakWifiIsEnabled()) {
        stakWifiEnable();
        e.component<Blips>()->startAnim();
        ms.displayLabel("wifi on");
      } else {
        stakWifiDisable();
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
    bat.assign<Power>();
    bat.replace<PressHandler>([](MenuSystem &ms, Entity e) {
      auto power = e.component<Power>();
      power->isCharging = stakPowerIsCharging();

      e.component<DetailView>()->press();
    });
    bat.replace<ReleaseHandler>([](MenuSystem &ms, Entity e) {
      e.component<DetailView>()->release();
    });
    bat.replace<DrawHandler>([](Entity e) {
      auto detail = e.component<DetailView>();

      auto power = e.component<Power>();
      power->percentCharged = stakPowerPercent();

      if (detail->generalScale > 0.0f) {
        scale(detail->generalScale);

        ScopedMask mask(display.bounds.size);
        {
          ScopedTransform xf;
          translate(display.bounds.size * -0.5f);

          beginMask();
          draw(mode.iconBatteryMask);
          endMask();

          beginPath();
          rect(display.bounds);
          fillColor(vec3(0.35f));
          fill();
        }

        beginPath();
        float t = mode.time * 2.0f;
        float y = -48.0f + (power->percentCharged / 100.f) * 96.0f;
        moveTo(-48.0f, y + std::sin(t) / M_PI * 10.0f);
        lineTo(48.0f, y + std::cos(t) / M_PI * 10.0f);
        lineTo(48, -48);
        lineTo(-48, -48);
        fillColor(0, 1, 0);
        fill();

        if (power->isCharging) {
          translate(display.bounds.size * -0.5f);
          draw(mode.iconCharging);
        }
      }

      if (detail->detailScale > 0.0f) {
        scale(detail->detailScale);

        fillColor(vec3(1));

        pushTransform();
        translate(0, 8);
        textAlign(ALIGN_CENTER | ALIGN_BASELINE);
        fontSize(20);
        char percentText[5];
        sprintf(percentText, "%.1f%%", power->percentCharged);
        fillText(percentText);
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
    auto drawBytes = [](uint64_t bytes) {
      auto mb = formatMebibytes(bytes);
      fillTextCenteredWithSuffix(mb.first, mb.second, 21, 14);
    };

    auto mem = makeMenuItem(mode.entities, mode.rootMenu);
    mem.assign<Label>("memory");
    mem.assign<Bubbles>(Rect(15, 18, 65, 56), 8.0f);
    mem.assign<DetailView>();
    mem.assign<DiskSpace>();
    mem.replace<PressHandler>([](MenuSystem &ms, Entity e) {
      e.component<DetailView>()->press();
    });
    mem.replace<ReleaseHandler>([](MenuSystem &ms, Entity e) {
      e.component<DetailView>()->release();
    });
    mem.replace<SelectHandler>([](MenuSystem &ms, Entity e) {
      MenuItem::defaultHandleSelect(ms, e);
      auto ds = e.component<DiskSpace>();
      ds->used = stakDiskUsage();
      ds->total = stakDiskSize();
      e.component<Bubbles>()->setPercent(double(ds->used) / double(ds->total));
    });
    mem.replace<DrawHandler>([=](Entity e) {
      auto detail = e.component<DetailView>();

      if (detail->generalScale > 0.0f) {
        scale(detail->generalScale);

        ScopedMask mask(display.bounds.size);
        translate(display.bounds.size * -0.5f);

        beginMask();
        draw(mode.iconMemoryMask);
        endMask();

        beginPath();
        rect(display.bounds);
        fillColor(vec3(0.35f));
        fill();

        e.component<Bubbles>()->draw();
      }

      if (detail->detailScale > 0.0f) {
        auto ds = e.component<DiskSpace>();

        scale(detail->detailScale);

        fillColor(vec3(1));

        pushTransform();
        translate(0, 8);
        drawBytes(ds->used);
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
        drawBytes(ds->total);
        popTransform();
      }
    });
  }

  display.wake();

  return 0;
}

STAK_EXPORT int shutdown() {
  return 0;
}

STAK_EXPORT int update(float dt) {
  display.update([dt] {
    mode.time += dt;

    timeline.step(dt);
    mode.systems.update<MenuSystem>(dt);

    mode.frameCount++;

    mode.secondsPerFrame += dt;
    if (mode.frameCount % 60 == 0) {
      std::cout << (1.0f / (mode.secondsPerFrame / 60.0f)) << " fps" << std::endl;
      mode.secondsPerFrame = 0.0f;
    }
  });
  return 0;
}

STAK_EXPORT int draw() {
  display.draw([] {
    mode.systems.system<MenuSystem>()->draw();
  });
  return 0;
}

STAK_EXPORT int crank_rotated(int amount) {
  mode.systems.system<MenuSystem>()->turn(amount * -0.25f);
  display.wake();
  return 0;
}

STAK_EXPORT int shutter_button_pressed() {
  if (!display.wake()) mode.systems.system<MenuSystem>()->pressItem();
  return 0;
}

STAK_EXPORT int shutter_button_released() {
  auto ms = mode.systems.system<MenuSystem>();
  ms->releaseAndActivateItem();
  display.wake();
  return 0;
}

STAK_EXPORT int power_button_pressed() {
  mode.systems.system<MenuSystem>()->indicatePreviousMenu();
  display.wake();
  return 0;
}

STAK_EXPORT int power_button_released() {
  mode.systems.system<MenuSystem>()->activatePreviousMenu();
  display.wake();
  return 0;
}

STAK_EXPORT int crank_pressed() {
  display.wake();
  return 0;
}

STAK_EXPORT int crank_released() {
  display.wake();
  return 0;
}
