#include "menu.hpp"

using namespace choreograph;
using namespace glm;

namespace otto {

const vec3 MenuItem::defaultColor = { 0.0f, 1.0f, 1.0f };
const vec3 MenuItem::defaultActiveColor = { 1.0f, 1.0f, 0.0f };

void MenuItem::defaultHandleDraw(const MenuItem &item) {
  beginPath();
  circle(vec2(), 44);
  fillColor(item.color());
  fill();
}

void MenuItem::defaultHandleSelect(MenuSystem &ms, MenuItem &item) {
  timeline.apply(&item.color).then<RampTo>(defaultActiveColor, 0.2f, EaseOutQuad());
  timeline.apply(&item.scale).then<RampTo>(1.0f, 0.2f, EaseOutQuad());
}

void MenuItem::defaultHandleDeselect(MenuSystem &ms, MenuItem &item) {
  timeline.apply(&item.color).then<RampTo>(defaultColor, 0.2f, EaseInOutQuad());
  timeline.apply(&item.scale).then<RampTo>(0.8f, 0.2f, EaseInOutQuad());
}

void MenuItem::defaultHandlePress(MenuSystem &ms, MenuItem &item) {
  timeline.apply(&item.scale).then<RampTo>(0.8f, 0.25f, EaseOutQuad());
  timeline.apply(&item.color).then<RampTo>(vec3(1, 0, 0), 0.25f, EaseOutQuad());
}

void MenuItem::defaultHandleRelease(MenuSystem &ms, MenuItem &item) {
  timeline.apply(&item.scale).then<RampTo>(1.0f, 0.25f, EaseOutQuad());
  timeline.apply(&item.color).then<RampTo>(defaultActiveColor, 0.25f, EaseOutQuad());
}

void MenuItem::defaultHandleActivate(MenuSystem &ms, MenuItem &item) {
  if (item.subMenu) ms.activateMenu(item.subMenu.get());
}

MenuItem *Menu::makeItem() {
  items.emplace_back(new MenuItem());
  return items.back().get();
}

void Menu::draw() const {
  auto radius = regularPolyRadius(tileRadius * 2.2f, items.size());
  auto angleIncr = -TWO_PI / items.size();

  pushTransform();
  translate(position() + vec2(radius, 0.0f));
  rotate(rotation.angle);

  auto drawItem = [&](size_t i) {
    const auto &item = *items[i];
    pushTransform();
    rotate(float(i) / items.size() * -TWO_PI);
    translate(-radius, 0.0f);
    scale(item.scale());
    item.handleDraw(item);
    popTransform();
  };

  drawItem(currentIndex);

  float offset = indexedRotation - currentIndex;
  if (offset < -0.25f) drawItem((items.size() + currentIndex - 1) % items.size());
  if (offset > 0.25f) drawItem((currentIndex + 1) % items.size());

  popTransform();
}

MenuSystem::MenuSystem(const vec2 &screenSize) : screenSize{ screenSize } {
}

void MenuSystem::step() {
  auto &menu = *mActiveMenu;

  menu.rotation.friction = menu.activeItem ? 0.3f : 0.15f;
  menu.rotation.step();

  menu.indexedRotation = menu.rotation.angle / TWO_PI * menu.items.size();
  menu.currentIndex = std::fmod(std::round(menu.indexedRotation), menu.items.size());

  auto timeSinceLastCrank = std::chrono::steady_clock::now() - menu.lastCrankTime;
  if (timeSinceLastCrank > std::chrono::milliseconds(350)) {
    if (!menu.activeItem) {
      menu.activeItem = menu.items[menu.currentIndex].get();
      if (menu.activeItem->handleSelect) {
        menu.activeItem->handleSelect(*this, *menu.activeItem);
      }
    }
    menu.rotation.lerp(float(menu.currentIndex) / menu.items.size() * TWO_PI, 0.2f);
  }
}

void MenuSystem::draw() {
  translate(screenSize * 0.5f);

  if (mDeactivatingMenu) mDeactivatingMenu->draw();
  mActiveMenu->draw();
}

void MenuSystem::turn(float amount) {
  auto &menu = *mActiveMenu;

  menu.rotation.angle += amount / menu.items.size();
  menu.lastCrankTime = std::chrono::steady_clock::now();

  if (menu.activeItem) {
    if (menu.activeItem->handleDeselect) {
      menu.activeItem->handleDeselect(*this, *menu.activeItem);
    }
    menu.activeItem = nullptr;
  }
}

void MenuSystem::activateMenu(Menu *menu, bool pushToStack) {
  // NOTE(ryan): Bail if there's already an activation in progress. We do this here to make the
  // user-facing API less error prone.
  if (mDeactivatingMenu) return;

  float direction = pushToStack ? 1.0f : -1.0f;

  // Deactivate previously active menu and animate out
  if (mActiveMenu) {
    mDeactivatingMenu = mActiveMenu;

    timeline.apply(&mDeactivatingMenu->position)
        .then<RampTo>(vec2(-screenSize.x * direction, 0.0f), 0.3f, EaseInOutQuad())
        .finishFn([&](Motion<vec2> &m) { mDeactivatingMenu = nullptr; });

    if (pushToStack) {
      mMenuStack.push_back(mDeactivatingMenu);
    }
  }

  // Animate in the new active menu
  menu->position = vec2(screenSize.x * direction, 0.0f);
  timeline.apply(&menu->position).then<RampTo>(vec2(), 0.3f, EaseInOutQuad());
  mActiveMenu = menu;
}

void MenuSystem::activateMenu(Menu *menu) {
  activateMenu(menu, true);
}

void MenuSystem::activatePreviousMenu() {
  if (!mDeactivatingMenu && mMenuStack.size()) {
    activateMenu(mMenuStack.back(), false);
    mMenuStack.pop_back();
  }
}

void MenuSystem::pressItem() {
  auto activeItem = mActiveMenu->activeItem;
  if (activeItem && activeItem->handlePress) activeItem->handlePress(*this, *activeItem);
}

void MenuSystem::releaseItem() {
  auto activeItem = mActiveMenu->activeItem;
  if (activeItem && activeItem->handleRelease) activeItem->handleRelease(*this, *activeItem);
}

void MenuSystem::activateItem() {
  auto activeItem = mActiveMenu->activeItem;
  if (activeItem && activeItem->handleActivate) activeItem->handleActivate(*this, *activeItem);
}


} // otto
