#pragma once

#include "timeline.hpp"
#include "gfx.hpp"
#include "util.hpp"

#include "entityx/entityx.h"

#include <chrono>
#include <functional>
#include <vector>

namespace otto {

using entityx::Entity;
using entityx::System;
using entityx::ComponentHandle;

struct Position {
  ch::Output<glm::vec2> position;
  Position(const glm::vec2 &position = {}) : position{ position } {}
};

struct Rotation : public AngularParticle {};

struct Scale {
  ch::Output<glm::vec2> scale;
  Scale(const glm::vec2 &scale = glm::vec2(1.0f)) : scale{ scale } {}
};

struct Color {
  ch::Output<glm::vec3> color;
  Color(const glm::vec3 &color = {}) : color{ color } {}
};

struct Label {
  using LabelFn = std::function<std::string(Entity)>;

  LabelFn getLabel;

  Label(const LabelFn &getLabel) : getLabel{ getLabel } {}
  Label(const std::string &label) : getLabel{ [=](Entity e) { return label; } } {}
};

class MenuSystem;

#define MAKE_HANDLER(NAME, FN_TYPE, FN_NAME)                                                       \
  struct NAME {                                                                                    \
    using HandlerFn = std::function<FN_TYPE>;                                                      \
    HandlerFn FN_NAME;                                                                             \
    NAME(const HandlerFn &FN_NAME) : FN_NAME{ FN_NAME } {}                                         \
  };

MAKE_HANDLER(DrawHandler, void(Entity), draw);
MAKE_HANDLER(SelectHandler, void(MenuSystem &, Entity), select);
MAKE_HANDLER(DeselectHandler, void(MenuSystem &, Entity), deselect);
MAKE_HANDLER(PressHandler, void(MenuSystem &, Entity), press);
MAKE_HANDLER(ReleaseHandler, void(MenuSystem &, Entity), release);
MAKE_HANDLER(ActivateHandler, void(MenuSystem &, Entity), activate);

#undef MAKE_HANDLER

struct Menu {
  static void defaultHandleDraw(Entity entity);

  std::vector<Entity> items;
  Entity activeItem;

  float indexedRotation;
  size_t currentIndex;

  float tileRadius = 48.0f;

  std::chrono::steady_clock::time_point lastCrankTime;
};

struct MenuItem {
  static const glm::vec3 defaultColor;
  static const glm::vec3 defaultActiveColor;

  static void defaultHandleDraw(Entity entity);
  static void defaultHandleSelect(MenuSystem &ms, Entity entity);
  static void defaultHandleDeselect(MenuSystem &ms, Entity entity);
  static void defaultHandlePress(MenuSystem &ms, Entity entity);
  static void defaultHandleRelease(MenuSystem &ms, Entity entity);
  static void defaultHandleActivate(MenuSystem &ms, Entity entity);

  Entity subMenu;
};

class MenuSystem : public System<MenuSystem> {
  std::vector<Entity> mMenuStack;

  Entity mActiveMenu;
  Entity mDeactivatingMenu;

  std::string mLabelText;
  ch::Output<float> mLabelOpacity = 0.0f;

  void activateMenu(Entity menuEntity, bool pushToStack);

public:
  glm::vec2 screenSize;

  MenuSystem(const glm::vec2 &screenSize);

  void update(entityx::EntityManager &es, entityx::EventManager &events,
              entityx::TimeDelta dt) override;
  void draw();
  void turn(float amount);

  void activateMenu(Entity menuEntity);
  void activatePreviousMenu();

  void pressItem();
  void releaseItem();
  void activateItem();

  void displayLabel(const std::string &text, float duration = 0.5f);
  void displayLabelInfinite(const std::string &text);
  void hideLabel();
};

Entity makeMenu(entityx::EntityManager &es);
Entity makeMenuItem(entityx::EntityManager &es, Entity menuEntity);

} // otto
