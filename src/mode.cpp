#include "gfx.hpp"

#include <iostream>

extern "C" {
  int init();
  int shutdown();
  int update();
  int rotary_changed(int delta);
}

static float rotation = 0.0f;

int init() {
  std::cout << "HELLO!!!" << std::endl;
  return 0;
}

int shutdown() {
  return 0;
}

int update() {
  using namespace otto;

  static const float screenWidth  = 96.0f;
  static const float screenHeight = 96.0f;
  static const float defaultMatrix[] =
    { -0.0f, -1.0f, -0.0f, -1.0f,  0.0f,  0.0f, screenWidth, screenHeight, 1.0f };

  static const VGfloat bgColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

  vgSetfv(VG_CLEAR_COLOR, 4, bgColor);
  vgClear(0, 0, 96, 96);

  vgLoadMatrix(defaultMatrix);
  vgTranslate(48, 48);
  vgRotate(rotation);

  {
    auto path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);

    moveTo(path, 0, 0);
    lineTo(path, 48, 0);

    strokeWidth(5.0f);
    strokeColor(1, 1, 0);

    vgDrawPath(path, VG_STROKE_PATH);
    vgDestroyPath(path);
  }

  return 0;
}

int rotary_changed(int delta) {
  rotation += delta * 5.0f;
  return 0;
}
