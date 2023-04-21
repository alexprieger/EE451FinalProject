#include <jni.h>
#include <android/log.h>
#include <string>
#include <sstream>
#include <vector>
#include <vulkan/vulkan.h>

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

struct Pixel {
    int row;
    int col;
};

struct PixelDirection {
    struct Pixel pixel;
    Direction direction;
};

Direction nextClockWiseDirection(Direction startDirection) {
    return static_cast<Direction>((startDirection + 1) % 4);
}

Direction nextCounterClockWiseDirection(Direction startDirection) {
    // Add 3 instead of subtracting 1 to avoid negative modulus weirdness
    return static_cast<Direction>((startDirection + 3) % 4);
}

// Returns the next 1-pixel clockwise from startDirection and its direction relative to the center pixel,
// or {0, 0} if there is no such 1-pixel
struct PixelDirection scanClockwise(const jbyte* imageBuffer, jint width, Pixel centerPixel, Direction startDirection) {
    Direction nextDirection = nextClockWiseDirection(startDirection);
    while (nextDirection != startDirection) {
        switch (nextDirection) {
            case left:
                if (imageBuffer[centerPixel.row * width + centerPixel.col - 1] != 0) {
                    return {{centerPixel.row, centerPixel.col - 1}, left};
                }
                break;
            case up:
                if (imageBuffer[(centerPixel.row - 1) * width + centerPixel.col] != 0) {
                    return {{centerPixel.row - 1, centerPixel.col}, up};
                }
                break;
            case right:
                if (imageBuffer[centerPixel.row * width + centerPixel.col + 1] != 0) {
                    return {{centerPixel.row, centerPixel.col + 1}, right};
                }
                break;
            case down:
                if (imageBuffer[(centerPixel.row + 1) * width + centerPixel.col] != 0) {
                    return {{centerPixel.row + 1, centerPixel.col}, down};
                }
                break;
        }
        nextDirection = nextClockWiseDirection(nextDirection);
    }
    return {0, 0}; // No pixel found: (0, 0) cannot possibly be a neighbor of a 1-pixel
}

// Returns the next 1-pixel counterclockwise from startDirection and the relative direction to the center pixel
struct PixelDirection scanCounterClockwise(const jbyte* imageBuffer, jint width, Pixel centerPixel, Direction startDirection, bool& isBorderingToRight) {
    isBorderingToRight = false;
    Direction nextDirection = nextCounterClockWiseDirection(startDirection);
    while (true) { // One of these is guaranteed to be hit within four tries
        switch (nextDirection) {
            case left:
                if (imageBuffer[centerPixel.row * width + centerPixel.col - 1] != 0) {
                    return {{centerPixel.row, centerPixel.col - 1}, right};
                }
                break;
            case up:
                if (imageBuffer[(centerPixel.row - 1) * width + centerPixel.col] != 0) {
                    return {{centerPixel.row - 1, centerPixel.col}, down};
                }
                break;
            case right:
                if (imageBuffer[centerPixel.row * width + centerPixel.col + 1] != 0) {
                    return {{centerPixel.row, centerPixel.col + 1}, left};
                } else {
                    isBorderingToRight = true;
                }
                break;
            case down:
                if (imageBuffer[(centerPixel.row + 1) * width + centerPixel.col] != 0) {
                    return {{centerPixel.row + 1, centerPixel.col}, up};
                }
                break;
        }
        nextDirection = nextCounterClockWiseDirection(nextDirection);
    }
}

jobject getJavaEdgeListFromEdgeList(JNIEnv *env, const std::vector<std::vector<Pixel>>& edgeList) {
    jclass vectorClass = env->FindClass("java/util/Vector");
    if(vectorClass == nullptr) {
        return nullptr;
    }

    jclass pixelClass = env->FindClass("com/ajp/ee451finalproject/MainActivity$Pixel");
    if(pixelClass == nullptr) {
        return nullptr;
    }

    jmethodID vectorConstructorID = env->GetMethodID(
            vectorClass, "<init>", "()V");
    if(vectorConstructorID == nullptr) {
        return nullptr;
    }

    jmethodID addMethodID = env->GetMethodID(
            vectorClass, "add", "(Ljava/lang/Object;)Z" );
    if(addMethodID == nullptr) {
        return nullptr;
    }

    jmethodID pixelConstructorID = env->GetMethodID(pixelClass, "<init>", "(II)V");
    if(pixelConstructorID == nullptr) {
        return nullptr;
    }

    // Outer vector
    jobject outerVector = env->NewObject(vectorClass, vectorConstructorID);
    if(outerVector == nullptr) {
        return nullptr;
    }

    for(auto& edge : edgeList) {

        // Inner vector
        jobject innerVector = env->NewObject(vectorClass, vectorConstructorID);
        if(innerVector == nullptr) {
            return nullptr;
        }

        for(auto& pixel : edge) {
            // Now, we have object created by Pixel(i, i)
            jobject pixelValue = env->NewObject(pixelClass, pixelConstructorID, pixel.row, pixel.col);
            if(pixelValue == nullptr) {
                return nullptr;
            }

            env->CallBooleanMethod(innerVector, addMethodID, pixelValue);
        }

        env->CallBooleanMethod(outerVector, addMethodID, innerVector);

    }

    env->DeleteLocalRef(vectorClass);
    env->DeleteLocalRef(pixelClass);

    return outerVector;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_ajp_ee451finalproject_MainActivity_suzukiEdgeFind(JNIEnv *env, jobject thiz,
                                                           jbyteArray image, jint width,
                                                           jint height) {
    // Image array is in row major order, with 0 representing black and 1 representing white
    jbyte* imageBuffer = env->GetByteArrayElements(image, nullptr);
    int numberOfBorders = 1;
    std::vector<std::vector<Pixel>> edgeList;
    // Perform a raster scan of the entire image.
    // We do not iterate over the (entirely black) frame, which conveniently avoids wraparound issues
    for (int row = 1; row < height - 1; row++) {
        for (int col = 1; col < width - 1; col++) {
            struct Pixel startPixel = {row, col};
            struct PixelDirection zeroPixel{};
            if (imageBuffer[row * width + col] == 1 && imageBuffer[row * width + col - 1] == 0) {
                zeroPixel = {row, col - 1, left};
            } else if (imageBuffer[row * width + col] >= 1 && imageBuffer[row * width + col + 1] == 0) {
                zeroPixel = {row, col + 1, right};
            } else {
                continue;
            }
            numberOfBorders++;
            edgeList.emplace_back();
            // Border following initialization: find the first two pixels
            struct PixelDirection retValue = scanClockwise(imageBuffer, width, startPixel, zeroPixel.direction);
            // Direction, once initialized, always points towards previous pixel
            struct PixelDirection currentPixel = {startPixel};
            currentPixel.direction = retValue.direction;
            struct Pixel startPrevPixel = retValue.pixel;
            if (startPrevPixel.row == 0 && startPrevPixel.col == 0) {
                // It's just a single isolated pixel, we're already done.
                edgeList.back().push_back(currentPixel.pixel);
                continue;
            }
            // Now following a border
            while (true) {
                edgeList.back().push_back(currentPixel.pixel);
                bool isBorderingToRight;
                struct PixelDirection nextPixel = scanCounterClockwise(imageBuffer, width, currentPixel.pixel, currentPixel.direction, isBorderingToRight);
                if (isBorderingToRight) {
                    // We need to specially mark the pixel if it's the border we're currently following
                    // faces to the right, to prevent us from accidentally following it again assuming it's a hole border
                    imageBuffer[currentPixel.pixel.row * width + currentPixel.pixel.col] = -1;
                } else if (imageBuffer[currentPixel.pixel.row * width + currentPixel.pixel.col] == 1) {
                    imageBuffer[currentPixel.pixel.row * width + currentPixel.pixel.col] = 2;
                }
                // If we're back at the starting point, we've found the entire border
                if (nextPixel.pixel.row == startPixel.row && nextPixel.pixel.col == startPixel.col
                    && currentPixel.pixel.row == startPrevPixel.row && currentPixel.pixel.col == startPrevPixel.col) {
                    break;
                }
                currentPixel = nextPixel;
            }
        }
    }
    env->ReleaseByteArrayElements(image, imageBuffer, JNI_ABORT);
    jobject retVal = getJavaEdgeListFromEdgeList(env, edgeList);
    return retVal;
}