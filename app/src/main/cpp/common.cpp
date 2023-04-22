#include <jni.h>
#include <vector>

#include "common.h"

Direction nextClockWiseDirection(Direction startDirection) {
    return static_cast<Direction>((startDirection +
#ifdef CONNECTIVITY_8
    1
#endif
#ifndef CONNECTIVITY_8
    2
#endif
    ) % 8);
}

Direction nextCounterClockWiseDirection(Direction startDirection) {
    // Add 3 instead of subtracting 1 to avoid negative modulus weirdness
    return static_cast<Direction>((startDirection +
#ifdef CONNECTIVITY_8
    7
#endif
#ifndef CONNECTIVITY_8
    6
#endif
    ) % 8);
}