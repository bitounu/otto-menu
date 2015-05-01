#include "stak.h"
#include "otto/devices/disk.hpp"
#include "otto/devices/power.hpp"
#include "otto/devices/wifi.hpp"
#include "otto/system.hpp"

#include "display.hpp"
#include "util.hpp"
#include "math.hpp"
#include "menu.hpp"
#include "rand.hpp"
#include "draw.hpp"
#include "fx.hpp"
#include "ottdate.hpp"

#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include "entityx/entityx.h"

#include <chrono>
#include <iostream>
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <sys/stat.h>

using namespace choreograph;
using namespace otto;

static const float pi = M_PI;
static const float twoPi = M_PI * 2.0f;
static const float halfPi = M_PI / 2.0f;

static const float detailDurationMin = 1.0f;

static std::thread infoPollingThread;
static volatile bool running = true;

std::mutex info_mutex;

static struct WifiInfo {
  std::mutex info_mutex;
  std::string ip;
  std::string ssid;

  const std::string get_ssid() {
    std::lock_guard<std::mutex> lock( info_mutex );
    return ssid;

  }
  void set_ssid( const std::string& new_ssid ) {
    std::lock_guard<std::mutex> lock( info_mutex );
    ssid = new_ssid;

  }
  const std::string get_ip() {
    std::lock_guard<std::mutex> lock( info_mutex );
    return ip;

  }
  void set_ip( const std::string& new_ip ) {
    std::lock_guard<std::mutex> lock( info_mutex );
    ip = new_ip;

  }
} wifiInfo;


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
struct Update {
  OttDate* updater = OttDate::instance();
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


std::string pipe_to_string( const char* command )
{
    FILE* file = popen( command, "r" );

    if( file )
    {
        std::ostringstream stm;

        constexpr std::size_t MAX_LINE_SZ = 1024;
        char line[MAX_LINE_SZ];

        while( fgets( line, MAX_LINE_SZ, file ) ) stm << line;

        pclose(file);
        std::string returnData = stm.str();
        returnData.erase(std::remove_if(returnData.begin(),
                              returnData.end(),
                              [](char x){ return ( (x == '\n')||(x=='\r') ); }),
               returnData.end());
        return returnData;
    }

    return "";
}

static bool wifiState = false;

STAK_EXPORT int init() {
  wifiState = ottoWifiIsEnabled();
  auto assets = std::string(stak_assets_path());

  mkdir( "/mnt/tmp", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );
  mkdir( "/mnt/pictures", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );

  running = true;
  wifiInfo.set_ssid(std::string(""));
  wifiInfo.set_ip(std::string(""));
  auto t = std::thread( [] {
    auto ssid_command = "iwconfig wlan1 | grep ESSID | cut -d\\\" -f 2";
    auto ip_command_eth1 = "ip addr show eth1 | grep -E \"inet\\s\" | awk '{ print $2 }' | grep -oE \"[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\"";
    auto ip_command_wlan0 = "ip addr show wlan0 | grep -E \"inet\\s\" | awk '{ print $2 }' | grep -oE \"[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\"";
    while( running ) {
      {
        auto retval = pipe_to_string( ssid_command );
        if( !retval.empty() ) {
          wifiInfo.set_ssid( std::string(retval) );
        } else{
          wifiInfo.set_ssid( std::string("") );
        }
      }
      {
        std::string retval = pipe_to_string( ip_command_wlan0 );
        auto ip_string = std::string("");
        if( !retval.empty() ) {
          ip_string = retval;
        } else{
          retval = pipe_to_string( ip_command_eth1 );
          if( !retval.empty() ) {
            ip_string = retval;
          }
        }
        wifiInfo.set_ip( ip_string );
      }
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
  });
  infoPollingThread = std::move(t);
  loadFont(assets + "232MKSD-round-medium.ttf");

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
    wifi.assign<DetailView>();
    wifi.replace<DrawHandler>([](Entity e) {

      if( wifiState != ottoWifiIsEnabled() ) {
        wifiState = ottoWifiIsEnabled();
        if ( wifiState ) {
          e.component<Blips>()->startAnim();
          e.component<DetailView>()->press();
        }
        else {
          e.component<Blips>()->stopAnim();
          e.component<DetailView>()->release();
        }
      }
      auto detail = e.component<DetailView>();
      auto fillTextCentered =  [](const std::string &text, float textSize) {
        ScopedTransform xf;

        fontSize(textSize);
        auto textBounds = getTextBounds(text);

        textAlign(ALIGN_LEFT | ALIGN_BASELINE);
        translate(-0.5f * textBounds.size.x, 0);
        fontSize(textSize);
        fillText(text);
        translate(textBounds.size.x, 0);
      };
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
      if( !ottoWifiIsEnabled() ) {
          pushTransform();
          translate(0, -30);
          fontSize(18);
          textAlign(ALIGN_CENTER | ALIGN_BASELINE);
          fillColor(vec3(1));
          fillText( "OFF" );
          popTransform();
        }
      if (detail->detailScale > 0.0f) {
        auto ds = e.component<DiskSpace>();

        scale(detail->detailScale);

        pushTransform();
        translate(display.bounds.size * -0.5f);
        translate(0, 20);

        beginPath();
        rect(display.bounds);
        fillColor(0, 0, 0, 0.75f);
        fill();
        popTransform();

        fillColor(vec3(1));

        pushTransform();
        translate(0, 4);
        if( !wifiInfo.get_ssid().empty() )
          fillTextCentered( wifiInfo.get_ssid().c_str(), 10 );
        popTransform();


        pushTransform();
        translate(0, -8);
        beginPath();
        moveTo(-20, 4);
        lineTo(20, 4);
        strokeCap(VG_CAP_SQUARE);
        strokeWidth(2);
        strokeColor(vec3(0.35f));
        stroke();
        popTransform();

        pushTransform();
        translate(0, -18);
        if( !wifiInfo.get_ip().empty() )
          fillTextCentered( wifiInfo.get_ip().c_str(), 10 );
        popTransform();
      }




    });
    wifi.replace<ActivateHandler>([](MenuSystem &ms, Entity e) {
      if (!ottoWifiIsEnabled()) {
        ottoWifiEnable();
        //e.component<Blips>()->startAnim();
        //e.component<DetailView>()->press();
        //ms.displayLabel("wifi on");
      } else {
        ottoWifiDisable();
        //e.component<Blips>()->stopAnim();
        //e.component<DetailView>()->release();
        //ms.displayLabel("wifi off");
      }
    });
  }


  //
  // Update
  //
  {
    auto update = makeMenuItem(mode.entities, mode.rootMenu);
    update.assign<Label>("Update");

    update.replace<DrawHandler>([](Entity e) {

			//print current state
      fontSize(12);
      textAlign(ALIGN_CENTER | ALIGN_BASELINE);
      fillColor( vec3(1) );
			fillText( OttDate::instance()->state_name() );

			switch( OttDate::instance()->current_state() )
			{
			  case OttDate::EState_Downloading: {
					fontSize(18);
					translate(0, -20);
					static std::stringstream ss;
					ss.str("");
					ss<<OttDate::instance()->download_percentage();
					ss<<"%";
					fillText( ss.str() );
					translate(0, 20);
					//fillColor(vec4(colorBGR(0xEC008B), rewindMeterOpacity()));
   				drawProgressArc(display, (OttDate::instance()->download_percentage()%100) / 100.0);
					break;
				}
			}

    });

    update.replace<ActivateHandler>([](MenuSystem &ms, Entity e) {
				switch(OttDate::instance()->current_state()) {
					case OttDate::EState_Idle:
						OttDate::instance()->trigger_update();
						break;
					case OttDate::EState_AskForReboot:
						ms.displayLabel("Bye bye!");
						system("/sbin/reboot");
					break;
				default:
					ms.displayLabel("busy...");
				break;
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
      power->isCharging = ottoPowerIsCharging();
      if (power->isCharging)
        power->timeToCharged = ottoPowerTimeToFullyCharged();
      else
        power->timeToDepleted = ottoPowerTimeToDepletion();

      e.component<DetailView>()->press();
    });
    bat.replace<ReleaseHandler>([](MenuSystem &ms, Entity e) {
      e.component<DetailView>()->release();
    });
    bat.replace<DrawHandler>([](Entity e) {
      auto detail = e.component<DetailView>();

      auto power = e.component<Power>();
      power->percentCharged = ottoPowerPercent();

      if (detail->generalScale > 0.0f) {
        scale(detail->generalScale);

        ScopedMask mask(display.bounds.size);
        {
          ScopedTransform xf;
          translate(display.bounds.size * -0.5f);

          beginMask();
          drawSvg(mode.iconBatteryMask);
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
          drawSvg(mode.iconCharging);
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
      ds->used = ottoDiskUsage();
      ds->total = ottoDiskSize();
      e.component<Bubbles>()->setPercent(double(ds->used) / double(ds->total));
    });
    mem.replace<DrawHandler>([=](Entity e) {
      auto detail = e.component<DetailView>();

      if (detail->generalScale > 0.0f) {
        scale(detail->generalScale);

        ScopedMask mask(display.bounds.size);
        translate(display.bounds.size * -0.5f);

        beginMask();
        drawSvg(mode.iconMemoryMask);
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
        fillColor(vec4(vec3(0.35f), 1.0f - quadIn(t2 * 2.0f)));
        drawProgressArc(display, t);
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
                  ottoSystemShutdown();
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
  running = false;
  infoPollingThread.join();
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
  if (!display.wake() && !mode.isPoweringDown) stak_activate_mode();
  return 0;
}

STAK_EXPORT int power_button_released() {
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
