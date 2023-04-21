#include <jni.h>
#include <android/log.h>
#include <string>
#include <sstream>
#include <vulkan/vulkan.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_ajp_ee451finalproject_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ajp_ee451finalproject_MainActivity_vulkanHello(JNIEnv *env, jobject thiz) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = 0;

    VkInstance instance;

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
}

enum Direction {
    left = 0,
    up = 1,
    right = 2,
    down = 3
};

struct element {
    int row;
    int col;
    Direction direction;
};

Direction nextClockWiseDirection(Direction startDirection) {
    return static_cast<Direction>((startDirection + 1) % 4);
}

Direction nextCounterClockWiseDirection(Direction startDirection) {
    // Add 3 instead of subtracting 1 to avoid negative modulus weirdness
    return static_cast<Direction>((startDirection + 3) % 4);
}

struct element scanClockwise(const jbyte* imageBuffer, jint width, int row, int col, Direction startDirection) {
    Direction nextDirection = nextClockWiseDirection(startDirection);
    while (nextDirection != startDirection) {
        switch (nextDirection) {
            case left:
                if (imageBuffer[row * width + col - 1] != 0) {
                    return {row, col - 1, left};
                }
                break;
            case up:
                if (imageBuffer[(row - 1) * width + col] != 0) {
                    return {row - 1, col, up};
                }
                break;
            case right:
                if (imageBuffer[row * width + col + 1] != 0) {
                    return {row, col + 1, right};
                }
                break;
            case down:
                if (imageBuffer[(row + 1) * width + col] != 0) {
                    return {row + 1, col, down};
                }
                break;
        }
        nextDirection = nextClockWiseDirection(nextDirection);
    }
    return {0, 0}; // No pixel found: (0, 0) cannot possibly be a neighbor of a 1-pixel
}

struct element scanCounterClockwise(const jbyte* imageBuffer, jint width, int row, int col, Direction startDirection, bool& isBorderingToRight) {
    isBorderingToRight = false;
    Direction nextDirection = nextCounterClockWiseDirection(startDirection);
    while (true) { // One of these is guaranteed to be hit within four tries
        switch (nextDirection) {
            case left:
                if (imageBuffer[row * width + col - 1] != 0) {
                    return {row, col - 1, right};
                }
                break;
            case up:
                if (imageBuffer[(row - 1) * width + col] != 0) {
                    return {row - 1, col, down};
                }
                break;
            case right:
                if (imageBuffer[row * width + col + 1] != 0) {
                    return {row, col + 1, left};
                } else {
                    isBorderingToRight = true;
                }
                break;
            case down:
                if (imageBuffer[(row + 1) * width + col] != 0) {
                    return {row + 1, col, up};
                }
                break;
        }
        nextDirection = nextCounterClockWiseDirection(nextDirection);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ajp_ee451finalproject_MainActivity_processImageBitmap(JNIEnv *env, jobject thiz,
                                                               jbyteArray image, jint width,
                                                               jint height) {
    // Image array is in row major order, with 0 representing black and 1 representing white
    jbyte* imageBuffer = env->GetByteArrayElements(image, nullptr);
    int numberOfBorders = 1;
    // Perform a raster scan of the entire image.
    // We do not iterate over the (entirely black) frame, which conveniently avoids wraparound issues
    for (int row = 1; row < height - 1; row++) {
        for (int col = 1; col < width - 1; col++) {
            struct element startPixel = {row, col};
            struct element zeroPixel{};
            if (imageBuffer[row * width + col] == 1 && imageBuffer[row * width + col - 1] == 0) {
                zeroPixel = {row, col - 1, left};
            } else if (imageBuffer[row * width + col] >= 1 && imageBuffer[row * width + col + 1] == 0) {
                zeroPixel = {row, col + 1, right};
            } else {
                continue;
            }
            numberOfBorders++;
            // Border following initialization: find the first two pixels
            struct element startPrevPixel = scanClockwise(imageBuffer, width, row, col, zeroPixel.direction);
            if (startPrevPixel.row == 0 && startPrevPixel.col == 0) {
                continue;
            }
            struct element currentPixel = startPixel;
            currentPixel.direction = startPrevPixel.direction;
            struct element prevPixel = startPrevPixel;
            // Now following a border
            while (true) {
                bool isBorderingToRight;
                struct element nextPixel = scanCounterClockwise(imageBuffer, width, currentPixel.row, currentPixel.col, currentPixel.direction, isBorderingToRight);
                if (isBorderingToRight) {
                    // We need to specially mark the pixel if it's the border we're currently following
                    // faces to the right, to prevent us from accidentally following it again assuming it's a hole border
                    imageBuffer[currentPixel.row * width + currentPixel.col] = -numberOfBorders;
                } else if (imageBuffer[currentPixel.row * width + currentPixel.col] == 1) {
                    imageBuffer[currentPixel.row * width + currentPixel.col] = numberOfBorders;
                }
                // If we're back at the starting point, we've found the entire border
                if (nextPixel.row == startPixel.row && nextPixel.col == startPixel.col
                    && currentPixel.row == startPrevPixel.row && currentPixel.col == startPrevPixel.col) {
                    break;
                }
                prevPixel = currentPixel;
                currentPixel = nextPixel;
            }
        }
    }
    for (int row = 1; row < height - 1; row++) {
        std::stringstream rowString;
        for (int col = 1; col < width - 1; col++) {
            rowString << static_cast<int>(imageBuffer[row * width + col]) << " ";
        }
        __android_log_print(ANDROID_LOG_INFO, "Alex", "%s", rowString.str().c_str());
    }
    env->ReleaseByteArrayElements(image, imageBuffer, JNI_ABORT);
}