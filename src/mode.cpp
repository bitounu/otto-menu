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
#include "gtx/rotate_vector.hpp"
#include "entityx/entityx.h"

#include <chrono>
#include <iostream>

using namespace choreograph;
using namespace otto;

static const float pi = M_PI;
static const float twoPi = M_PI * 2.0f;
static const float halfPi = M_PI / 2.0f;

static const float detailDurationMin = 1.0f;

static struct MenuMode : public entityx::EntityX {
  Entity rootMenu;

  Svg *iconBatteryMask, *iconMemoryMask, *iconCharging;

  double time = 0.0;

  float secondsPerFrame;
  uint32_t frameCount = 0;

  bool isPoweringDown = false;
} mode;

static Display display = { { 96.0f, 96.0f } };

struct DiskSpace {
  uint64_t used, total;
};

struct Power {
  float percentCharged;
  uint64_t timeToDepleted, timeToCharged;
  bool isCharging;
};

struct Nap {
  Output<float> progress = 0.0f;
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
      if (power->isCharging)
        power->timeToCharged = stakPowerTimeToFullyCharged();
      else
        power->timeToDepleted = stakPowerTimeToDepletion();

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
        moveTo(-48.0f, y + std::sin(t) / pi * 10.0f);
        lineTo(48.0f, y + std::cos(t) / pi * 10.0f);
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
        auto timeText =
            formatMillis(power->isCharging ? power->timeToCharged : power->timeToDepleted);
        fillTextCenteredWithSuffix(timeText.first, timeText.second, 21, 14);
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

  //
  // Nap
  //
  {
    auto nap = makeMenuItem(mode.entities, mode.rootMenu);
    nap.assign<Label>("sleep");
    nap.assign<Nap>();
    nap.replace<DrawHandler>([](Entity e) {
      auto nap = e.component<Nap>();
      auto t = std::min(1.0f, nap->progress());
      auto ti = 1.0f - t;
      auto t2 = std::max(0.0f, nap->progress() - 1.0f);

      static auto elasticIn = EaseInElastic(1.0f, 1.0f);
      static auto quadIn = EaseInQuad();
      static auto quadOut = EaseOutQuad();
      static auto quadInOut = EaseInOutQuad();

      // Sun / Moon
      {
        const int tipCount = 22;
        const int vtxCount = tipCount * 2;
        const float radius = display.bounds.size.x * 0.3f;
        const float radiusTipOffset = radius * 0.15f;

        ScopedTransform xf;
        translate(vec2(0.0f, elasticIn(t2) * -display.bounds.size.y));
        rotate(std::sin(mode.time) * 0.3f * ti);
        scale(lerp(1.0f, 0.8f, t));

        // Body
        {
          beginPath();
          auto tipAmt = mapUnitClamp(t, 0.5f, 0.0f);
          vec2 p;
          for (int i = 0; i < vtxCount; ++i) {
            p = vec2(radius + tipAmt * radiusTipOffset * (i % 2 == 0 ? -1.0f : 1.0f), 0.0f);
            p = glm::rotate(p, float(i) / float(vtxCount) * twoPi);
            if (i == 0) moveTo(p); else lineTo(p);
          }
          fillColor(glm::mix(colorBGR(0xE7D11A), colorBGR(0x7DCED2), mapUnitClamp(t, 0.0f, 0.5f)));
          fill();
        }

        // Face
        {
          static const float faceSmile[] = {
            -14, 2,                                    // moveTo
            -13, 6, -8, 6, -7, 2,                      // cubicTo
            7, 2,                                      // moveTo
            8, 6, 13, 6, 14, 2,                        // cubicTo
            -10, -7.25,                                // moveTo
            -5.455, -13.584, 5.455, -13.584, 10, -7.25 // cubicTo
          };

          static const float faceSleep[] = {
            -14, 2,                                // moveTo
            -13, -0.666, -8, -0.666, -7, 2,        // cubicTo
            7, 2,                                  // moveTo
            8, -0.666, 13, -0.666, 14, 2,          // cubicTo
            -3, -9,                                // moveTo
            -1.637, -10.666, 1.636, -10.666, 3, -9 // cubicTo
          };

          // TODO(ryan): Clean this up and just build the VGPath objects directly.
          int i = 0;
          auto fp = [&] {
            return lerp(faceSmile[i], faceSleep[i++], quadInOut(t));
          };

          strokeColor(vec3(0));
          strokeWidth(3.0f);
          strokeCap(VG_CAP_ROUND);

          beginPath();
          for (int j = 0; j < 3; ++j) {
            moveTo(fp(), fp());
            cubicTo(fp(), fp(), fp(), fp(), fp(), fp());
          }
          stroke();
        }

        // Moon Shadow
        if (t > 0.5f) {
          ScopedTransform xf;
          rotate(pi * 0.25f);

          float r = radius + 1.0f;
          beginPath();
          moveTo(0, -r);
          lineTo(r, -r);
          lineTo(r, r);
          lineTo(0, r);
          float xscale = mapClamp(t, 0.5f, 1.0f, -1.0f, 1.0f);
          float amax = xscale > 0.0f ? pi + halfPi : -halfPi;
          arc(0, 0, 2.0f * r * quadOut(std::abs(xscale)), r * 2.0f, halfPi, amax);
          fillColor(0, 0, 0, 0.75f);
          fill();
        }
      }

      if (t > 0.0f) {
        const float width = 3.0f;
        const float inset = 12.0f;
        beginPath();
        arc(vec2(), display.bounds.size - inset, halfPi + t * -twoPi, halfPi);
        strokeColor(vec4(vec3(0.35f), 1.0f - quadIn(t2 * 2.0f)));
        strokeWidth(width);
        strokeCap(VG_CAP_BUTT);
        stroke();
      }
    });
    nap.replace<PressHandler>([](MenuSystem &ms, Entity e) {
      auto nap = e.component<Nap>();
      timeline.apply(&nap->progress)
          .then<RampTo>(1.0f, 2.0f)
          .finishFn([&ms, nap](Motion<float> &m) mutable {
            ms.displayLabel("good night");
            mode.isPoweringDown = true;
            timeline.apply(&nap->progress)
                .then<RampTo>(2.0f, 0.5f)
                .then<Hold>(2.0f, 1.0f)
                .finishFn([](Motion<float> &m) {
                  // ottoSystemShutdown();
                  exit(0); // TODO(ryan): This is temp. Remove when the ottoSystemShutdown() works.
                });
          });
    });
    nap.replace<ReleaseHandler>([](MenuSystem &ms, Entity e) {
      if (!mode.isPoweringDown)
        timeline.apply(&e.component<Nap>()->progress).then<RampTo>(0.0f, 0.25f);
    });
    nap.replace<DeselectHandler>([](MenuSystem &ms, Entity e) {
      if (!mode.isPoweringDown)
        timeline.apply(&e.component<Nap>()->progress).then<RampTo>(0.0f, 0.25f);
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
