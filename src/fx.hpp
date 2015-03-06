#pragma once

#include "gfx.hpp"
#include "timeline.hpp"

#include <vector>
#include <memory>

namespace otto {

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

  Bubbles(const Rect &bounds, float bubbleRadius);
  ~Bubbles();

  void startBubbleAnim(size_t i, bool delay);
  void stopBubbleAnim(size_t i);

  void setCount(size_t count);

  void draw();
};

struct Blip {
  ch::Output<vec3> color;
  ch::Output<float> scale = 0.0f;

  void draw();
};

struct Blips {
  std::vector<std::unique_ptr<Blip>> blips;
  Blip centerBlip;

  std::vector<vec3> colors = { colorBGR(0x00ADEF), colorBGR(0xEC008B), colorBGR(0xFFF100) };

  size_t nextColorIndex = 0;
  float blipRadius = 40.0f;
  bool animating = false;

  Blips();

  void startBlipAnim(Blip &blip, float delay);

  void startAnim();
  void stopAnim();

  void draw();
  void drawCenter();
};

} // otto
