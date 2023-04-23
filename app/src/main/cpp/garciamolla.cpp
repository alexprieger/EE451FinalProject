#include <jni.h>
#include <vulkan/vulkan.h>
#include <list>
#include <array>
#include <queue>
#include <cassert>

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

    bool operator==(struct Triad otherTriad) const {
        // Two triads are equal if they point in the same direction, because distinct borders never converge
        return currentPixel == otherTriad.currentPixel
                && nextPixel.direction == otherTriad.nextPixel.direction;
    }

    bool pointsTo(struct Triad otherTriad) const {
        return nextPixel.pixel == otherTriad.currentPixel
            && otherTriad.prevPixel.pixel == currentPixel;
    }
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

bool isBorderClosed(std::list<Triad>* border) {
    return
        // Border is just a single pixel
            (border->size() == 1 && border->front().nextPixel.pixel.row == 0)
            // ...or the border is a closed loop
            || (border->size() > 1
                && border->front() == border->back());
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
    // Follow border forward until we leave our segment or loop back around
    while (currentTriad.nextPixel.pixel.row >= startRow && currentTriad.nextPixel.pixel.row < endRow
           && currentTriad.nextPixel.pixel.col >= startCol && currentTriad.nextPixel.pixel.col < endCol) {
        PixelDirection nextNextPixel = scan(imageBuffer, width, currentTriad.nextPixel.pixel, currentTriad.nextPixel.direction, false);
        currentTriad = {currentTriad.nextPixel.pixel,
                        {currentTriad.currentPixel,reverseDirection(currentTriad.nextPixel.direction)},
                        nextNextPixel};
        border->push_back(currentTriad);
        if (currentTriad == startTriad) {
            // Border is closed
            return border;
        }
    }
    // Follow border backward until we leave our segment or loop back around
    currentTriad = startTriad;
    while (currentTriad.prevPixel.pixel.row >= startRow && currentTriad.prevPixel.pixel.row < endRow
           && currentTriad.prevPixel.pixel.col >= startCol && currentTriad.prevPixel.pixel.col < endCol) {
        PixelDirection prevPrevPixel = scan(imageBuffer, width, currentTriad.prevPixel.pixel, currentTriad.prevPixel.direction, true);
        currentTriad = {currentTriad.prevPixel.pixel,
                        prevPrevPixel,
                        {currentTriad.currentPixel, reverseDirection(currentTriad.prevPixel.direction)}};
        border->push_front(currentTriad);
        if (currentTriad == border->back()) {
            // Border is closed
            return border;
        }
    }
    return border;
}

struct BorderList {
    std::list<std::list<Triad> *>* closedBorders;
    std::list<std::list<Triad> *>* openBorders;
};

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
            if (imageBuffer[(row + 1) * width + col] != 0){
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

BorderList joinBorders(BorderList list1, BorderList list2) {
    // We'll operate in-place on the first list and add the second list to it
    list1.closedBorders->splice(list1.closedBorders->end(), *list2.closedBorders);
    // Join any open borders in list 2 with anything they can join with in list 1
    for (auto border2Iter = list2.openBorders->begin(); border2Iter != list2.openBorders->end();)
    {
        bool matchesWithBorder = false;
        for (auto border1Iter = list1.openBorders->begin(); border1Iter != list1.openBorders->end(); border1Iter++) {
            if ((*border1Iter)->back().pointsTo((*border2Iter)->front())) {
                matchesWithBorder = true;
                (*border1Iter)->splice((*border1Iter)->end(), **border2Iter);
                delete *border2Iter;
                border2Iter = list2.openBorders->erase(border2Iter);
                if ((*border1Iter)->back().pointsTo((*border1Iter)->front())) {
                    (*border1Iter)->push_back((*border1Iter)->front());
                    list1.closedBorders->splice(list1.closedBorders->end(), *(list1.openBorders), border1Iter);
                }
                break;
            } else if ((*border2Iter)->back().pointsTo((*border1Iter)->front())) {
                matchesWithBorder = true;
                (*border1Iter)->splice((*border1Iter)->begin(), **border2Iter);
                delete *border2Iter;
                border2Iter = list2.openBorders->erase(border2Iter);
                if ((*border1Iter)->back().pointsTo((*border1Iter)->front())) {
                    (*border1Iter)->push_back((*border1Iter)->front());
                    list1.closedBorders->splice(list1.closedBorders->end(), *(list1.openBorders), border1Iter);
                }
                break;
            }
        }
        if (!matchesWithBorder) {
            auto oldBorder2Iter = border2Iter;
            border2Iter++;
            list1.openBorders->splice(list1.openBorders->end(), *(list2.openBorders), oldBorder2Iter);
        }
    }
    // Joining borders may have resulted in new borders that can be joined. Check them all
    std::queue<std::list<std::list<Triad> *>::iterator> bordersToCheck; // queue or stack doesn't matter here, we just need to check them all
    for (auto borderIter = list1.openBorders->begin(); borderIter != list1.openBorders->end(); borderIter++) {
        bordersToCheck.push(borderIter);
    }
    while (!bordersToCheck.empty()) {
        auto borderToCheck = bordersToCheck.front();
        bordersToCheck.pop();
        for (auto borderToCheckAgainstIter = list1.openBorders->begin(); borderToCheckAgainstIter != list1.openBorders->end(); borderToCheckAgainstIter++) {
            std::list<std::list<Triad> *>::iterator* fromBorder = nullptr;
            std::list<std::list<Triad> *>::iterator* toBorder = nullptr;
            if ((*borderToCheck)->back().pointsTo((*borderToCheckAgainstIter)->front())) {
                fromBorder = &borderToCheck;
                toBorder = &borderToCheckAgainstIter;
            } else if ((*borderToCheckAgainstIter)->back().pointsTo((*borderToCheck)->front())) {
                fromBorder = &borderToCheckAgainstIter;
                toBorder = &borderToCheck;
            }
            if (fromBorder != nullptr) {
                (**fromBorder)->splice((**fromBorder)->end(), ***toBorder);
                delete **toBorder;
                list1.openBorders->erase(*toBorder);
                if ((**fromBorder)->back().pointsTo((**fromBorder)->front())) {
                    (**fromBorder)->push_back((**fromBorder)->front()); // Close the border
                    list1.closedBorders->splice(list1.closedBorders->end(), *(list1.openBorders), *fromBorder);
                } else {
                    bordersToCheck.push(*fromBorder);
                }
                break;
            }
        }
    }
    return list1;
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
        return joinBorders(list1, list2);
    }
    // More columns than rows, so split horizontally
    BorderList list1 = findBordersInRectangle(imageBuffer, width, startRow, endRow, startCol, (startCol + endCol) / 2);
    BorderList list2 = findBordersInRectangle(imageBuffer, width, startRow, endRow, (startCol + endCol) / 2, endCol);
    return joinBorders(list1, list2);
}

jobject getJavaEdgeListFromBorderList(JNIEnv *env, const BorderList& edgeList, double time) {
    jclass edgeDetectionResultClass = env->FindClass("com/ajp/ee451finalproject/EdgeDetectionActivity$EdgeDetectionResult");
    if(edgeDetectionResultClass == nullptr) {
        return nullptr;
    }

    jclass vectorClass = env->FindClass("java/util/Vector");
    if(vectorClass == nullptr) {
        return nullptr;
    }

    jclass pixelClass = env->FindClass("com/ajp/ee451finalproject/EdgeDetectionActivity$Pixel");
    if(pixelClass == nullptr) {
        return nullptr;
    }

    jmethodID edgeDetectionResultConstructorID = env->GetMethodID(edgeDetectionResultClass, "<init>",
                                                                  "(Ljava/util/Vector;D)V");
    if(edgeDetectionResultConstructorID == nullptr) {
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

    for(auto& edge : *(edgeList.closedBorders)) {

        // Inner vector
        jobject innerVector = env->NewObject(vectorClass, vectorConstructorID);
        if(innerVector == nullptr) {
            return nullptr;
        }

        for(auto& triad : *edge) {
            // Now, we have object created by Pixel(i, i)
            int row = triad.currentPixel.row;
            int col = triad.currentPixel.col;
            jobject pixelValue = env->NewObject(pixelClass, pixelConstructorID, row, col);
            if(pixelValue == nullptr) {
                return nullptr;
            }

            env->CallBooleanMethod(innerVector, addMethodID, pixelValue);
        }

        env->CallBooleanMethod(outerVector, addMethodID, innerVector);

    }

    jobject result = env->NewObject(edgeDetectionResultClass, edgeDetectionResultConstructorID, outerVector, time);

    env->DeleteLocalRef(edgeDetectionResultClass);
    env->DeleteLocalRef(vectorClass);
    env->DeleteLocalRef(pixelClass);

    return result;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_ajp_ee451finalproject_EdgeDetectionActivity_garciaMollaEdgeFind(JNIEnv *env, jobject thiz,
                                                                         jbyteArray image,
                                                                         jint width, jint height) {
    jbyte* imageBuffer = env->GetByteArrayElements(image, nullptr);
    struct timespec startTime{};
    clock_gettime(CLOCK_REALTIME, &startTime);
    BorderList borderList = findBordersInRectangle(imageBuffer, width, 0, height, 0, width);
    struct timespec endTime{};
    clock_gettime(CLOCK_REALTIME, &endTime);
    double timeMillis = (endTime.tv_sec - startTime.tv_sec + (endTime.tv_nsec - startTime.tv_nsec) / 1000000000.0) * 1000.0;
    assert(borderList.openBorders->empty());
    env->ReleaseByteArrayElements(image, imageBuffer, JNI_ABORT);
    jobject retVal = getJavaEdgeListFromBorderList(env, borderList, timeMillis);
    for (auto border : *(borderList.closedBorders)) {
        delete border;
    }
    for (auto border : *(borderList.openBorders)) {
        delete border;
    }
    delete borderList.closedBorders;
    delete borderList.openBorders;
    return retVal;
}