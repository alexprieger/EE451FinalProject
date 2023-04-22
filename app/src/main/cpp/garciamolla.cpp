#include <jni.h>
#include <vulkan/vulkan.h>
#include <list>
#include <array>

#include "common.h"

extern "C"
JNIEXPORT void JNICALL
Java_com_ajp_ee451finalproject_EdgeDetectionActivity_vulkanHello(JNIEnv *env, jobject thiz) {
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

struct Triad {
    Pixel currentPixel;
    PixelDirection prevPixel; // Direction points towards currentPixel
    PixelDirection nextPixel; // Direction points towards currentPixel
};

const jbyte LEFT_MASK = 0x02;
const jbyte UP_MASK = 0x04;
const jbyte RIGHT_MASK = 0x08;
const jbyte DOWN_MASK = 0x10;

Direction reverseDirection(Direction initialDirection) {
    switch(initialDirection) {
        case left:
            return right;
        case up:
            return down;
        case right:
            return left;
        case down:
            return up;
#ifdef CONNECTIVITY_8
        case upleft:
            return downright;
        case upright:
            return downleft;
        case downright:
            return upleft;
        case downleft:
            return upright;
#endif
    }
}

// Returns the next 1-pixel clockwise or counterclockwise from startDirection and its direction
// relative to the center pixel, or {0, 0} if there is no such 1-pixel
struct PixelDirection scan(jbyte* imageBuffer, jint width, Pixel centerPixel, Direction startDirection, bool scanClockwise) {
    Direction nextDirection;
    if (scanClockwise) {
        nextDirection = nextClockWiseDirection(startDirection);
    } else {
        nextDirection = nextCounterClockWiseDirection(startDirection);
    }
    while (nextDirection != startDirection) {
        switch (nextDirection) {
            case left:
                imageBuffer[centerPixel.row * width + centerPixel.col] |= LEFT_MASK;
                if (imageBuffer[centerPixel.row * width + centerPixel.col - 1] != 0) {
                    return {{centerPixel.row, centerPixel.col - 1}, right};
                }
                break;
            case up:
                imageBuffer[centerPixel.row * width + centerPixel.col] |= UP_MASK;
                if (imageBuffer[(centerPixel.row - 1) * width + centerPixel.col] != 0) {
                    return {{centerPixel.row - 1, centerPixel.col}, down};
                }
                break;
            case right:
                imageBuffer[centerPixel.row * width + centerPixel.col] |= RIGHT_MASK;
                if (imageBuffer[centerPixel.row * width + centerPixel.col + 1] != 0) {
                    return {{centerPixel.row, centerPixel.col + 1}, left};
                }
                break;
            case down:
                imageBuffer[centerPixel.row * width + centerPixel.col] |= DOWN_MASK;
                if (imageBuffer[(centerPixel.row + 1) * width + centerPixel.col] != 0) {
                    return {{centerPixel.row + 1, centerPixel.col}, up};
                }
                break;
#ifdef CONNECTIVITY_8
            case upleft:
                if (imageBuffer[(centerPixel.row - 1) * width + centerPixel.col - 1] != 0) {
                    return {{centerPixel.row - 1, centerPixel.col - 1}, downright};
                }
                break;
            case upright:
                if (imageBuffer[(centerPixel.row - 1) * width + centerPixel.col + 1] != 0) {
                    return {{centerPixel.row - 1, centerPixel.col + 1}, downleft};
                }
                break;
            case downright:
                if (imageBuffer[(centerPixel.row + 1) * width + centerPixel.col + 1] != 0) {
                    return {{centerPixel.row + 1, centerPixel.col + 1}, upleft};
                }
                break;
            case downleft:
                if (imageBuffer[(centerPixel.row + 1) * width + centerPixel.col - 1] != 0) {
                    return {{centerPixel.row + 1, centerPixel.col - 1}, upright};
                }
                break;
#endif
        }
        if (scanClockwise) {
            nextDirection = nextClockWiseDirection(nextDirection);
        } else {
            nextDirection = nextCounterClockWiseDirection(nextDirection);
        }
    }
    return {0, 0}; // No pixel found: (0, 0) cannot possibly be a 1-pixel
}

std::list<Triad> followBorder(jbyte* imageBuffer, jint width, int startRow, int endRow, int startCol, int endCol, Triad startTriad) {
    std::list<Triad> border;
    border.push_back(startTriad);
    Triad currentTriad = startTriad;
    // Follow border forward until we leave our segment or loop back around
    while (currentTriad.nextPixel.pixel.row >= startRow && currentTriad.nextPixel.pixel.row < endRow
           && currentTriad.nextPixel.pixel.col >= startCol && currentTriad.nextPixel.pixel.col < endCol
           && !(currentTriad.currentPixel.row == startTriad.currentPixel.row
            && currentTriad.currentPixel.col == startTriad.currentPixel.col
            && currentTriad.nextPixel.direction == startTriad.nextPixel.direction)) {
        PixelDirection nextNextPixel = scan(imageBuffer, width, currentTriad.nextPixel.pixel, currentTriad.nextPixel.direction, false);
        currentTriad = {currentTriad.nextPixel.pixel,
                        {currentTriad.currentPixel,reverseDirection(currentTriad.nextPixel.direction)},
                        nextNextPixel};
        border.push_back(currentTriad);
    }
    // If the loop is not closed yet, follow border backward
    if (!(border.front().currentPixel.row == border.back().currentPixel.row
         && border.front().currentPixel.col == border.back().currentPixel.col
         && border.front().nextPixel.direction == border.back().nextPixel.direction)) {
        // Follow border backward until we leave our segment or loop back around
        while (currentTriad.nextPixel.pixel.row >= startRow && currentTriad.nextPixel.pixel.row < endRow
               && currentTriad.nextPixel.pixel.col >= startCol && currentTriad.nextPixel.pixel.col < endCol
               && !(currentTriad.currentPixel.row == border.back().currentPixel.row
                    && currentTriad.currentPixel.col == border.back().currentPixel.col
                    && currentTriad.nextPixel.direction == border.back().nextPixel.direction)) {
            PixelDirection prevPrevPixel = scan(imageBuffer, width, currentTriad.prevPixel.pixel, currentTriad.prevPixel.direction, true);
            currentTriad = {currentTriad.prevPixel.pixel,
                            {currentTriad.currentPixel, (currentTriad.prevPixel.direction)},
                            prevPrevPixel};
            border.push_back(currentTriad);
        }
    }
    return border;
}

bool checkTriad(jbyte* imageBuffer, jint width, int startRow, int endRow, int startCol, int endCol, int row, int col, std::list<std::list<Triad>>& edgeList, jbyte triadBitmask, Direction triadDirection) {
    Pixel currentPixel = {row, col};
    if (!(imageBuffer[row * width + col] & triadBitmask)) {
        imageBuffer[row * width + col] |= triadBitmask;
        PixelDirection nextPixel = scan(imageBuffer, width, currentPixel, triadDirection, false);
        if (nextPixel.pixel.row == 0 && nextPixel.pixel.col == 0) {
            // No other pixel found, pixel is isolated
            edgeList.emplace_back();
            edgeList.back().push_back({currentPixel});
            return true;
        }
        PixelDirection prevPixel = scan(imageBuffer, width, currentPixel, triadDirection, true);
        Triad pixelTriad = {currentPixel, prevPixel, nextPixel};
        edgeList.push_back(
                followBorder(imageBuffer, width, startRow, endRow, startCol, endCol,
                             pixelTriad));
    }
    return false;
}

std::list<std::list<Triad>> littleSuzuki(jbyte* imageBuffer, jint width, int startRow, int endRow, int startCol, int endCol) {
    std::list<std::list<Triad>> edgeList;
    for (int row = startRow; row < endRow; row++) {
        for (int col = startCol; col < endCol; col++) {
            if (imageBuffer[row * width + col] == 0) {
                continue;
            }
            // Check left triad if it has not already been followed
            if (checkTriad(imageBuffer, width, startRow, endRow, startCol, endCol, row, col, edgeList, LEFT_MASK, left)) {
                continue;
            }
            // Check up triad if it has not already been followed
            if (checkTriad(imageBuffer, width, startRow, endRow, startCol, endCol, row, col, edgeList, UP_MASK, up)) {
                continue;
            }
            // Check right triad if it has not already been followed
            if (checkTriad(imageBuffer, width, startRow, endRow, startCol, endCol, row, col, edgeList, RIGHT_MASK, right)) {
                continue;
            }
            // Check down triad if it has not already been followed
            if (checkTriad(imageBuffer, width, startRow, endRow, startCol, endCol, row, col, edgeList, DOWN_MASK, down)) {
                continue;
            }
        }
    }
}

std::list<std::list<Triad>> findBordersInRectangle(jbyte* imageBuffer, jint width, int startRow, int endRow, int startCol, int endCol) {
    if (endRow - startRow <= 4 && endCol - startCol <= 4) {
        return {};
    }
    // Process block [startRow, endRow) × [startCol, endCol)
    if (endRow - startRow >= endCol - startCol) {
        // More rows than columns, so split vertically
        std::list<std::list<Triad>> list1 = findBordersInRectangle(imageBuffer, width, startRow, (startRow + endRow) / 2, startCol, endCol);
        std::list<std::list<Triad>> list2 = findBordersInRectangle(imageBuffer, width, (startRow + endRow) / 2, endRow, startCol, endCol);
        list1.splice(list1.end(), list2);
        return list1;
    }
    // More columns than rows, so split horizontally
    std::list<std::list<Triad>> list1 = findBordersInRectangle(imageBuffer, width, startRow, endRow, startCol, (startCol + endCol) / 2);
    std::list<std::list<Triad>> list2 = findBordersInRectangle(imageBuffer, width, startRow, endRow, (startCol + endCol) / 2, endCol);
    list1.splice(list1.end(), list2);
    return list1;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_ajp_ee451finalproject_EdgeDetectionActivity_garciaMollaEdgeFind(JNIEnv *env, jobject thiz,
                                                                         jbyteArray image,
                                                                         jint width, jint height) {
    jbyte* imageBuffer = env->GetByteArrayElements(image, nullptr);
    findBordersInRectangle(imageBuffer, width, 0, width, 0, height);
    env->ReleaseByteArrayElements(image, imageBuffer, JNI_ABORT);
}