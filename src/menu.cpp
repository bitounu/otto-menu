#include "menu.hpp"

using namespace choreograph;
using namespace glm;

namespace otto {

const vec3 MenuItem::defaultColor = { 0.0f, 1.0f, 1.0f };
const vec3 MenuItem::defaultActiveColor = { 1.0f, 1.0f, 0.0f };

void MenuItem::defaultHandleDraw(Entity entity) {
  beginPath();
  circle(vec2(), 44);
  fillColor(entity.component<Color>()->color());
  fill();
}

void MenuItem::defaultHandleSelect(MenuSystem &ms, Entity entity) {
  timeline.apply(&entity.component<Color>()->color)
      .then<RampTo>(defaultActiveColor, 0.2f, EaseOutQuad());
  timeline.apply(&entity.component<Scale>()->scale).then<RampTo>(1.0f, 0.2f, EaseOutQuad());
}

void MenuItem::defaultHandleDeselect(MenuSystem &ms, Entity entity) {
  timeline.apply(&entity.component<Color>()->color)
      .then<RampTo>(defaultColor, 0.2f, EaseInOutQuad());
  timeline.apply(&entity.component<Scale>()->scale).then<RampTo>(0.8f, 0.2f, EaseInOutQuad());
}

void MenuItem::defaultHandlePress(MenuSystem &ms, Entity entity) {
  timeline.apply(&entity.component<Color>()->color)
      .then<RampTo>(vec3(1, 0, 0), 0.25f, EaseOutQuad());
  timeline.apply(&entity.component<Scale>()->scale).then<RampTo>(0.8f, 0.25f, EaseOutQuad());
}

void MenuItem::defaultHandleRelease(MenuSystem &ms, Entity entity) {
  timeline.apply(&entity.component<Scale>()->scale).then<RampTo>(1.0f, 0.25f, EaseOutQuad());
  timeline.apply(&entity.component<Color>()->color)
      .then<RampTo>(defaultActiveColor, 0.25f, EaseOutQuad());
}

void MenuItem::defaultHandleActivate(MenuSystem &ms, Entity entity) {
  auto item = entity.component<MenuItem>();
  if (item->subMenu) ms.activateMenu(item->subMenu);
}

void Menu::defaultHandleDraw(Entity entity) {
  auto menu = entity.component<Menu>();
  const auto &menuItems = menu->items;

  auto radius = regularPolyRadius(menu->tileRadius * 2.2f, menuItems.size());
  auto angleIncr = -TWO_PI / menuItems.size();

  pushTransform();
  translate(entity.component<Position>()->position() + vec2(radius, 0.0f));
  rotate(entity.component<Rotation>()->angle);

  auto drawItem = [&](size_t i) {
    if (i >= menuItems.size()) return;
    auto item = menuItems[i];
    auto handler = item.component<DrawHandler>();
    if (handler) {
      pushTransform();
      rotate(float(i) / menuItems.size() * -TWO_PI);
      translate(-radius, 0.0f);
      scale(item.component<Scale>()->scale());
      handler->draw(item);
      popTransform();
    }
  };

  drawItem(menu->currentIndex);

  float offset = menu->indexedRotation - menu->currentIndex;
  if (offset < -0.25f) {
    drawItem((menuItems.size() + menu->currentIndex - 1) % menuItems.size());
  }
  if (offset > 0.25f) {
    drawItem((menu->currentIndex + 1) % menuItems.size());
  }

  popTransform();
}

MenuSystem::MenuSystem(const vec2 &screenSize) : screenSize{ screenSize } {
}

void MenuSystem::update(entityx::EntityManager &es, entityx::EventManager &events,
                        entityx::TimeDelta dt) {
  auto menu = mActiveMenu.component<Menu>();

  auto rotation = mActiveMenu.component<Rotation>();
  rotation->friction = menu->activeItem ? 0.3f : 0.15f;
  rotation->step();

  menu->indexedRotation = rotation->angle / TWO_PI * menu->items.size();
  menu->currentIndex = std::fmod(std::round(menu->indexedRotation), menu->items.size());

  auto timeSinceLastCrank = std::chrono::steady_clock::now() - menu->lastCrankTime;
  if (timeSinceLastCrank > std::chrono::milliseconds(350)) {
    if (!menu->activeItem && menu->items.size() > 0) {
      menu->activeItem = menu->items[menu->currentIndex];
      auto itemHandleSelect = menu->activeItem.component<SelectHandler>();
      if (itemHandleSelect) {
        itemHandleSelect->select(*this, menu->activeItem);
      }
    }
    rotation->lerp(float(menu->currentIndex) / menu->items.size() * TWO_PI, 0.2f);
  }
}

void MenuSystem::draw() {
  translate(screenSize * 0.5f);

  if (mDeactivatingMenu) {
    mDeactivatingMenu.component<DrawHandler>()->draw(mDeactivatingMenu);
  }
  mActiveMenu.component<DrawHandler>()->draw(mActiveMenu);
}

void MenuSystem::turn(float amount) {
  auto menu = mActiveMenu.component<Menu>();

  mActiveMenu.component<Rotation>()->angle += amount / menu->items.size();
  menu->lastCrankTime = std::chrono::steady_clock::now();

  if (menu->activeItem) {
    auto itemHandleDeselect = menu->activeItem.component<DeselectHandler>();
    if (itemHandleDeselect) {
      itemHandleDeselect->deselect(*this, menu->activeItem);
    }
    menu->activeItem.invalidate();
  }
}

void MenuSystem::activateMenu(Entity menuEntity, bool pushToStack) {
  // NOTE(ryan): Bail if there's already an activation in progress. We do this here to make the
  // user-facing API less error prone.
  if (mDeactivatingMenu) return;

  float direction = pushToStack ? 1.0f : -1.0f;

  // Deactivate previously active menu and animate out
  if (mActiveMenu) {
    mDeactivatingMenu = mActiveMenu;

    timeline.apply(&mDeactivatingMenu.component<Position>()->position)
        .then<RampTo>(vec2(-screenSize.x * direction, 0.0f), 0.3f, EaseInOutQuad())
        .finishFn([&](Motion<vec2> &m) { mDeactivatingMenu.invalidate(); });

    if (pushToStack) {
      mMenuStack.push_back(mDeactivatingMenu);
    }
  }

  // Animate in the new active menu
  auto menu = menuEntity.component<Menu>();
  auto menuPos = menuEntity.component<Position>();
  menuPos->position = vec2(screenSize.x * direction, 0.0f);
  timeline.apply(&menuPos->position).then<RampTo>(vec2(), 0.3f, EaseInOutQuad());

  mActiveMenu = menuEntity;
}

void MenuSystem::activateMenu(Entity menuEntity) {
  activateMenu(menuEntity, true);
}

void MenuSystem::activatePreviousMenu() {
  if (!mDeactivatingMenu && mMenuStack.size()) {
    activateMenu(mMenuStack.back(), false);
    mMenuStack.pop_back();
  }
}

void MenuSystem::pressItem() {
  auto activeItem = mActiveMenu.component<Menu>()->activeItem;
  if (activeItem) {
    auto handler = activeItem.component<PressHandler>();
    if (handler) handler->press(*this, activeItem);
  }
}

void MenuSystem::releaseItem() {
  auto activeItem = mActiveMenu.component<Menu>()->activeItem;
  if (activeItem) {
    auto handler = activeItem.component<ReleaseHandler>();
    if (handler) handler->release(*this, activeItem);
  }
}

void MenuSystem::activateItem() {
  auto activeItem = mActiveMenu.component<Menu>()->activeItem;
  if (activeItem) {
    auto handler = activeItem.component<ActivateHandler>();
    if (handler) handler->activate(*this, activeItem);
  }
}

Entity makeMenu(entityx::EntityManager &es) {
  auto entity = es.create();

  entity.assign<Menu>();
  entity.assign<Position>();
  entity.assign<Rotation>();
  entity.assign<DrawHandler>(Menu::defaultHandleDraw);

  return entity;
}

Entity makeMenuItem(entityx::EntityManager &es, Entity menuEntity) {
  auto entity = es.create();

  entity.assign<MenuItem>();
  entity.assign<Scale>(0.8f);
  entity.assign<Color>(MenuItem::defaultColor);
  entity.assign<DrawHandler>(MenuItem::defaultHandleDraw);
  entity.assign<SelectHandler>(MenuItem::defaultHandleSelect);
  entity.assign<DeselectHandler>(MenuItem::defaultHandleDeselect);
  entity.assign<PressHandler>(MenuItem::defaultHandlePress);
  entity.assign<ReleaseHandler>(MenuItem::defaultHandleRelease);
  entity.assign<ActivateHandler>(MenuItem::defaultHandleActivate);

  menuEntity.component<Menu>()->items.emplace_back(entity);

  return entity;
}

} // otto