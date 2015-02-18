#pragma once

#include "timeline.hpp"
#include "gfx.hpp"
#include "util.hpp"

#include <chrono>
#include <functional>
#include <vector>

namespace otto {

class Menu;
class MenuSystem;

struct MenuItem {
  static const glm::vec3 defaultColor;
  static const glm::vec3 defaultActiveColor;

  ch::Output<glm::vec3> color = defaultColor;
  ch::Output<float> scale = 0.8f;

  static void defaultHandleDraw(const MenuItem &item);
  static void defaultHandleSelect(MenuSystem &ms, MenuItem &item);
  static void defaultHandleDeselect(MenuSystem &ms, MenuItem &item);
  static void defaultHandlePress(MenuSystem &ms, MenuItem &item);
  static void defaultHandleRelease(MenuSystem &ms, MenuItem &item);
  static void defaultHandleActivate(MenuSystem &ms, MenuItem &item);

  std::function<void(const MenuItem &)> handleDraw = defaultHandleDraw;
  std::function<void(MenuSystem &ms, MenuItem &)> handleSelect = defaultHandleSelect;
  std::function<void(MenuSystem &ms, MenuItem &)> handleDeselect = defaultHandleDeselect;
  std::function<void(MenuSystem &ms, MenuItem &)> handlePress = defaultHandlePress;
  std::function<void(MenuSystem &ms, MenuItem &)> handleRelease = defaultHandleRelease;
  std::function<void(MenuSystem &ms, MenuItem &)> handleActivate = defaultHandleActivate;

  std::unique_ptr<Menu> subMenu;
};

struct Menu {
  ch::Output<glm::vec2> position;
  AngularParticle rotation;

  std::vector<std::unique_ptr<MenuItem>> items;
  MenuItem *activeItem = nullptr;

  float indexedRotation;
  size_t currentIndex;

  float tileRadius = 48.0f;

  std::chrono::steady_clock::time_point lastCrankTime;

  MenuItem *makeItem();

  void draw() const;
};

class MenuSystem {
  std::vector<Menu *> mMenuStack;

  Menu *mActiveMenu = nullptr;
  Menu *mDeactivatingMenu = nullptr;

  void activateMenu(Menu *menu, bool pushToStack);

public:
  glm::vec2 screenSize;

  MenuSystem(const glm::vec2 &screenSize);

  void step();
  void draw();
  void turn(float amount);

  void activateMenu(Menu *menu);
  void activatePreviousMenu();

  void pressItem();
  void releaseItem();
  void activateItem();
};

} // otto
