#pragma once

#include "gfx.hpp"

namespace otto {

int32_t randInt();
int32_t randInt(int32_t v);
int32_t randInt(int32_t a, int32_t b);

float randFloat();
float randFloat(float v);
float randFloat(float a, float b);

vec2 randVec2(const Rect &rect);

} // otto
