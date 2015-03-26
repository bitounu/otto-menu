#include "fx.hpp"
#include "rand.hpp"

#include <algorithm>
#include <iostream>

using namespace choreograph;

namespace otto {

Bubbles::Bubbles(const Rect &bounds, float bubbleRadius)
: bounds(bounds),
  bubbleRadius{ bubbleRadius },
  circlePath{ vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0,
                           VG_PATH_CAPABILITY_ALL) } {
  size_t maxBubbles = bounds.getArea() / (M_PI * (bubbleRadius * bubbleRadius)) * 2.0f;
  bubbles.resize(maxBubbles);

  circle(circlePath, 0, 0, bubbleRadius);
  // setCount(0);
}
Bubbles::~Bubbles() {
  vgDestroyPath(circlePath);
}

void Bubbles::startBubbleAnim(size_t i, float delay) {
  auto &bubble = bubbles[i];
  bubble.position = randVec2(bounds);
  bubble.color = colors[randInt(colors.size())];
  timeline.apply(&bubble.scale)
      .then<Hold>(0.0f, delay)
      .then<RampTo>(1.0f, 1.0f, EaseOutQuad())
      .then<RampTo>(0.0f, 1.0f, EaseInQuad())
      .finishFn([=](ch::Motion<float> &m) { startBubbleAnim(i); });
}

void Bubbles::stopBubbleAnim(size_t i) {
  std::cout << "stop " << i << std::endl;
  timeline.apply(&bubbles[i].scale).then<RampTo>(0.0f, 1.0f, EaseOutQuad());
}

void Bubbles::setCount(size_t count) {
  for (size_t i = count; i < bubbleCount; ++i) stopBubbleAnim(i);
  for (size_t i = bubbleCount; i < count; ++i) startBubbleAnim(i, i * 0.1f);
  bubbleCount = count;
}

void Bubbles::setPercent(float percent) {
  setCount(percent * bubbles.size());
}

void Bubbles::draw() {
  for (size_t i = 0; i < bubbleCount; ++i) {
    const auto &bubble = bubbles[i];
    ScopedTransform xf;
    translate(bubble.position);
    scale(bubble.scale());
    fillColor(bubble.color);
    vgDrawPath(circlePath, VG_FILL_PATH);
  }
}

void Blip::draw() {
  beginPath();
  circle(0, 0, scale());
  fillColor(color());
  fill();
}

Blips::Blips() {
  for (size_t i = 0; i < 2; ++i) {
    blips.push_back(std::make_unique<Blip>());
  }
  centerBlip.color = vec3(0.35f);
  centerBlip.scale = 7.0f;
}

void Blips::startBlipAnim(Blip &blip, float delay) {
  const auto color = colors[nextColorIndex];

  timeline.apply(&blip.scale)
      .then<Hold>(7.0f, delay)
      .onInflection([=](ch::Motion<float> &m) {
        timeline.apply(&centerBlip.color)
            .then<Hold>(glm::mix(color, vec3(1), 0.75f), 0.0f)
            .then<RampTo>(vec3(0.35f), 0.5f, EaseInQuad());
      })
      .then<RampTo>(blipRadius, 2.0f, EaseOutQuad())
      .then<Hold>(0.0f, 0.0f)
      .finishFn([this, &blip](ch::Motion<float> &m) {
        std::rotate(blips.begin(), blips.begin() + 1, blips.end());
        if (animating) startBlipAnim(blip, 0.0f);
      });
  timeline.apply(&blip.color)
      .then<Hold>(color, delay + 1.0f)
      .then<RampTo>(vec3(), 1.0f, EaseOutQuad());

  nextColorIndex = (nextColorIndex + 1) % colors.size();
}

void Blips::startAnim() {
  animating = true;
  for (size_t i = 0; i < blips.size(); ++i) {
    startBlipAnim(*blips[i], float(i) / blips.size() * 2.0f);
  }
}

void Blips::stopAnim() {
  animating = false;
}

void Blips::draw() {
  for (const auto &blip : blips) {
    blip->draw();
  }
}
void Blips::drawCenter() {
  centerBlip.draw();
}

} // otto
