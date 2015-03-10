#pragma once

#include "choreograph/Choreograph.h"

namespace otto {

// TODO(ryan): It's probably not a super great idea to just have a single global instance of
// Timeline, but it's currently needed in a lot of places, and this is the simplest thing for now.
extern ch::Timeline timeline;

} // otto
