#include <jni.h>

// If defined, uses 8-connectivity instead of 4-connectivity
#define CONNECTIVITY_8

enum Direction {
    left = 0,
    up = 2,
    right = 4,
    down = 6,
#ifdef CONNECTIVITY_8
    upleft = 1,
    upright = 3,
    downright = 5,
    downleft = 7
#endif
};

struct Pixel {
    int row;
    int col;
};

struct PixelDirection {
    struct Pixel pixel;
    Direction direction;
};

Direction nextClockWiseDirection(Direction startDirection);
Direction nextCounterClockWiseDirection(Direction startDirection);