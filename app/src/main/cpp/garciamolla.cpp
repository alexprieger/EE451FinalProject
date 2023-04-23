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
    for (int i = 0; i <
#ifdef CONNECTIVITY_8
        8
#endif
#ifndef CONNECTIVITY_8
        4
#endif
    ; i++) {
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

std::list<Triad>* followBorder(jbyte* imageBuffer, jint width, int startRow, int endRow, int startCol, int endCol, Triad startTriad) {
    auto* border = new std::list<Triad>();
    border->push_back(startTriad);
    Triad currentTriad = startTriad;
    bool finished = false;
    // Follow border forward until we leave our segment or loop back around
    while (currentTriad.nextPixel.pixel.row >= startRow && currentTriad.nextPixel.pixel.row < endRow
           && currentTriad.nextPixel.pixel.col >= startCol && currentTriad.nextPixel.pixel.col < endCol
           && !finished) {
        PixelDirection nextNextPixel = scan(imageBuffer, width, currentTriad.nextPixel.pixel, currentTriad.nextPixel.direction, false);
        currentTriad = {currentTriad.nextPixel.pixel,
                        {currentTriad.currentPixel,reverseDirection(currentTriad.nextPixel.direction)},
                        nextNextPixel};
        border->push_back(currentTriad);
        finished = (currentTriad.currentPixel.row == startTriad.currentPixel.row
                    && currentTriad.currentPixel.col == startTriad.currentPixel.col
                    && currentTriad.nextPixel.direction == startTriad.nextPixel.direction);
    }
    // Follow border backward until we leave our segment or loop back around
    // If we already looped, while condition will evaluate false and never run
    currentTriad = startTriad;
    while (currentTriad.prevPixel.pixel.row >= startRow && currentTriad.prevPixel.pixel.row < endRow
           && currentTriad.prevPixel.pixel.col >= startCol && currentTriad.prevPixel.pixel.col < endCol
           && !(currentTriad.currentPixel.row == border->back().currentPixel.row
                && currentTriad.currentPixel.col == border->back().currentPixel.col
                && currentTriad.prevPixel.direction == border->back().prevPixel.direction)) {
        PixelDirection prevPrevPixel = scan(imageBuffer, width, currentTriad.prevPixel.pixel, currentTriad.prevPixel.direction, true);
        currentTriad = {currentTriad.prevPixel.pixel,
                        prevPrevPixel,
                        {currentTriad.currentPixel, reverseDirection(currentTriad.prevPixel.direction)}};
        border->push_front(currentTriad);
    }
    return border;
}

struct BorderList {
    std::list<std::list<Triad> *>* closedBorders;
    std::list<std::list<Triad> *>* openBorders;
};

bool isBorderClosed(std::list<Triad>* border) {
    return
    // Border is just a single pixel
    (border->size() == 1 && border->front().nextPixel.pixel.row == 0)
    // ...or the border is a closed loop
    || (border->size() > 1
    && border->front().currentPixel.row == border->back().currentPixel.row
    && border->front().currentPixel.col == border->back().currentPixel.col
    && border->front().nextPixel.direction == border->back().nextPixel.direction);
}

bool checkTriad(jbyte* imageBuffer, jint width, int startRow, int endRow, int startCol, int endCol, int row, int col, BorderList edgeList, jbyte triadBitmask, Direction triadDirection) {
    Pixel currentPixel = {row, col};
    switch (triadDirection) {
        case left:
            if (imageBuffer[row * width + col - 1] != 0){
                return false;
            }
            break;
        case up:
            if (imageBuffer[(row - 1) * width + col] != 0){
                return false;
            }
            break;
        case right:
            if (imageBuffer[row * width + col + 1] != 0){
                return false;
            }
            break;
        case down:
            if (imageBuffer[(row + 1) * width + col - 1] != 0){
                return false;
            }
            break;
    }
    if (!(imageBuffer[row * width + col] & triadBitmask)) {
        imageBuffer[row * width + col] |= triadBitmask;
        PixelDirection nextPixel = scan(imageBuffer, width, currentPixel, triadDirection, false);
        if (nextPixel.pixel.row == 0 && nextPixel.pixel.col == 0) {
            // No other pixel found, pixel is isolated
            edgeList.closedBorders->push_back(new std::list<Triad>());
            edgeList.closedBorders->back()->push_back({currentPixel});
            return true;
        }
        PixelDirection prevPixel = scan(imageBuffer, width, currentPixel, triadDirection, true);
        Triad pixelTriad = {currentPixel, prevPixel, nextPixel};
        auto* border = followBorder(imageBuffer, width, startRow, endRow, startCol, endCol,pixelTriad);
        if (isBorderClosed(border)) {
            edgeList.closedBorders->push_back(border);
        } else {
            edgeList.openBorders->push_back(border);
        }
    }
    return false;
}

BorderList littleSuzuki(jbyte* imageBuffer, jint width, int startRow, int endRow, int startCol, int endCol) {
    BorderList edgeList = {new std::list<std::list<Triad> *>(), new std::list<std::list<Triad> *>()};
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
    return edgeList;
}

BorderList findBordersInRectangle(jbyte* imageBuffer, jint width, int startRow, int endRow, int startCol, int endCol) {
    if (endRow - startRow <= 4 && endCol - startCol <= 4) {
        return littleSuzuki(imageBuffer, width, startRow, endRow, startCol, endCol);
    }
    // Process block [startRow, endRow) × [startCol, endCol)
    if (endRow - startRow >= endCol - startCol) {
        // More rows than columns, so split vertically
        BorderList list1 = findBordersInRectangle(imageBuffer, width, startRow, (startRow + endRow) / 2, startCol, endCol);
        BorderList list2 = findBordersInRectangle(imageBuffer, width, (startRow + endRow) / 2, endRow, startCol, endCol);
        return list1;
    }
    // More columns than rows, so split horizontally
    BorderList list1 = findBordersInRectangle(imageBuffer, width, startRow, endRow, startCol, (startCol + endCol) / 2);
    BorderList list2 = findBordersInRectangle(imageBuffer, width, startRow, endRow, (startCol + endCol) / 2, endCol);
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