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
};

struct Rotation : public AngularParticle {};

struct Scale {
  ch::Output<float> scale;
};

struct Color {
  ch::Output<glm::vec3> color;
};

class MenuSystem;

struct DrawHandler {
  std::function<void(Entity)> draw;
};
struct SelectHandler {
  std::function<void(MenuSystem &, Entity)> select;
};
struct DeselectHandler {
  std::function<void(MenuSystem &, Entity)> deselect;
};
struct PressHandler {
  std::function<void(MenuSystem &, Entity)> press;
};
struct ReleaseHandler {
  std::function<void(MenuSystem &, Entity)> release;
};
struct ActivateHandler {
  std::function<void(MenuSystem &, Entity)> activate;
};

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
};

Entity makeMenu(entityx::EntityManager &es);
Entity makeMenuItem(entityx::EntityManager &es, Entity menuEntity);

} // otto
