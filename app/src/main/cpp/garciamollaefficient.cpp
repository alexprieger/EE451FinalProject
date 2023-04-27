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

struct Border;

struct Triad {
    Pixel currentPixel;
    // The "general" pointers point to the FIRST (left) triad in the set of other-pixel triads
    // Initialized by connectTriads
    struct Triad* prevPixelGeneral;
    // If isValid is true and nextPixelGeneral is a null pointer, this pixel is isolated
    struct Triad* nextPixelGeneral; // Direction points towards currentPixel
    // The "general" pointers point to the next triad in the border
    // Initialized by getStartBorders
    struct Triad* prevPixelSpecific;
    struct Triad* nextPixelSpecific;
    std::list<Border>::iterator border;
    // False if the corresponding cross pixel is not black, or if this triad would be duplicated by another
    bool isValid;
    bool hasBeenFollowed;

    bool pointsTo(struct Triad* otherTriad) const {
        return nextPixelSpecific == otherTriad && otherTriad->prevPixelSpecific == this;
    }
};

struct Border {
    struct Triad* startTriad;
    struct Triad* endTriad;
};

struct BorderList {
    std::list<Border>* closedBorders;
    std::list<Border>* openBorders;
};

#define CROSS_LEFT 0
#define CROSS_UP 1
#define CROSS_RIGHT 2
#define CROSS_DOWN 3

#define LEFT_MASK 0x02
#define UP_MASK 0x04
#define RIGHT_MASK 0x08
#define DOWN_MASK 0x10

// Returns the next 1-pixel clockwise or counterclockwise from startDirection, or {0, 0} if there is no such 1-pixel
struct Pixel scan(jbyte* imageBuffer, jint width, Pixel centerPixel, Direction startDirection, bool scanClockwise) {
    Direction nextDirection;
    if (scanClockwise) {
        nextDirection = nextClockWiseDirection(startDirection);
    } else {
        nextDirection = nextCounterClockWiseDirection(startDirection);
    }
    for (int i = 0; i < 8; i++) {
        switch (nextDirection) {
            case left:
                imageBuffer[centerPixel.row * width + centerPixel.col] |= LEFT_MASK;
                if (imageBuffer[centerPixel.row * width + centerPixel.col - 1] != 0) {
                    return {centerPixel.row, centerPixel.col - 1};
                }
                break;
            case up:
                imageBuffer[centerPixel.row * width + centerPixel.col] |= UP_MASK;
                if (imageBuffer[(centerPixel.row - 1) * width + centerPixel.col] != 0) {
                    return {centerPixel.row - 1, centerPixel.col};
                }
                break;
            case right:
                imageBuffer[centerPixel.row * width + centerPixel.col] |= RIGHT_MASK;
                if (imageBuffer[centerPixel.row * width + centerPixel.col + 1] != 0) {
                    return {centerPixel.row, centerPixel.col + 1};
                }
                break;
            case down:
                imageBuffer[centerPixel.row * width + centerPixel.col] |= DOWN_MASK;
                if (imageBuffer[(centerPixel.row + 1) * width + centerPixel.col] != 0) {
                    return {centerPixel.row + 1, centerPixel.col};
                }
                break;
            case upleft:
                if (imageBuffer[(centerPixel.row - 1) * width + centerPixel.col - 1] != 0) {
                    return {centerPixel.row - 1, centerPixel.col - 1};
                }
                break;
            case upright:
                if (imageBuffer[(centerPixel.row - 1) * width + centerPixel.col + 1] != 0) {
                    return {centerPixel.row - 1, centerPixel.col + 1};
                }
                break;
            case downright:
                if (imageBuffer[(centerPixel.row + 1) * width + centerPixel.col + 1] != 0) {
                    return {centerPixel.row + 1, centerPixel.col + 1};
                }
                break;
            case downleft:
                if (imageBuffer[(centerPixel.row + 1) * width + centerPixel.col - 1] != 0) {
                    return {centerPixel.row + 1, centerPixel.col - 1};
                }
                break;
        }
        if (scanClockwise) {
            nextDirection = nextClockWiseDirection(nextDirection);
        } else {
            nextDirection = nextCounterClockWiseDirection(nextDirection);
        }
    }
    return {0, 0}; // No pixel found: (0, 0) cannot possibly be a 1-pixel
}

void connectTriads(Triad* triadBuffer, jbyte* imageBuffer, const int size) {
    for (int currentPixelRow = 1; currentPixelRow < size - 1; currentPixelRow++) {
        for (int currentPixelColumn = 1; currentPixelColumn < size - 1; currentPixelColumn++) {
            if (imageBuffer[currentPixelRow * size + currentPixelColumn] == 0) {
                // Ignore 0-pixels
                continue;
            }
            Pixel currentPixel = {currentPixelRow, currentPixelColumn};
            Triad *localPixelTriadBuffer = triadBuffer + currentPixelRow * size * 4 + currentPixelColumn * 4;
            for (int i = 0; i < 4; i++) {
                localPixelTriadBuffer[i].currentPixel = currentPixel;
                localPixelTriadBuffer[i].hasBeenFollowed = false;
            }
            // Get left triad
            if (imageBuffer[currentPixelRow * size + currentPixelColumn - 1] != 0) {
                // No left triad
                localPixelTriadBuffer[CROSS_LEFT].isValid = false;
            } else {
                Pixel leftTriadNext = scan(imageBuffer, size, currentPixel, left, false);
                if (leftTriadNext == Pixel{0, 0}) {
                    // Single, isolated pixel. Mark it as such
                    localPixelTriadBuffer[CROSS_LEFT].isValid = true;
                    localPixelTriadBuffer[CROSS_LEFT].nextPixelGeneral = nullptr;
                    localPixelTriadBuffer[CROSS_UP].isValid = false;
                    localPixelTriadBuffer[CROSS_RIGHT].isValid = false;
                    localPixelTriadBuffer[CROSS_DOWN].isValid = false;
                    continue;
                } else {
                    localPixelTriadBuffer[CROSS_LEFT].isValid = true;
                    localPixelTriadBuffer[CROSS_LEFT].nextPixelGeneral = triadBuffer + leftTriadNext.row * size * 4 + leftTriadNext.col * 4;
                    Pixel leftTriadPrev = scan(imageBuffer, size, currentPixel, left, true);
                    localPixelTriadBuffer[CROSS_LEFT].prevPixelGeneral = triadBuffer + leftTriadPrev.row * size * 4 + leftTriadPrev.col * 4;
                }
            }
            // Get up triad
            if (imageBuffer[(currentPixelRow - 1) * size + currentPixelColumn] != 0 || imageBuffer[currentPixelRow * size + currentPixelColumn] & UP_MASK) {
                // No up triad, or else it's the same as the left triad
                localPixelTriadBuffer[CROSS_UP].isValid = false;
            } else {
                Pixel upTriadNext = scan(imageBuffer, size, currentPixel, up, false);
                // Note thta we don't need to check if it's an isolated pixel because we would have found that out in the left triad
                localPixelTriadBuffer[CROSS_UP].isValid = true;
                localPixelTriadBuffer[CROSS_UP].nextPixelGeneral = triadBuffer + upTriadNext.row * size * 4 + upTriadNext.col * 4;
                Pixel leftTriadPrev = scan(imageBuffer, size, currentPixel, up, true);
                localPixelTriadBuffer[CROSS_UP].prevPixelGeneral = triadBuffer + leftTriadPrev.row * size * 4 + leftTriadPrev.col * 4;
            }
            // Get right triad
            if (imageBuffer[currentPixelRow * size + currentPixelColumn + 1] != 0 || imageBuffer[currentPixelRow * size + currentPixelColumn] & RIGHT_MASK) {
                // No right triad, or else it's the same as the left or up triad
                localPixelTriadBuffer[CROSS_RIGHT].isValid = false;
            } else {
                Pixel rightTriadNext = scan(imageBuffer, size, currentPixel, right, false);
                localPixelTriadBuffer[CROSS_RIGHT].isValid = true;
                localPixelTriadBuffer[CROSS_RIGHT].nextPixelGeneral = triadBuffer + rightTriadNext.row * size * 4 + rightTriadNext.col * 4;
                Pixel rightTriadPrev = scan(imageBuffer, size, currentPixel, right, true);
                localPixelTriadBuffer[CROSS_RIGHT].prevPixelGeneral = triadBuffer + rightTriadPrev.row * size * 4 + rightTriadPrev.col * 4;
            }
            // Get down triad
            if (imageBuffer[(currentPixelRow + 1) * size + currentPixelColumn] != 0 || imageBuffer[currentPixelRow * size + currentPixelColumn] & DOWN_MASK) {
                // No down triad, or else it's the same as the left, up, or right triad
                localPixelTriadBuffer[CROSS_DOWN].isValid = false;
            } else {
                Pixel downTriadNext = scan(imageBuffer, size, currentPixel, down, false);
                localPixelTriadBuffer[CROSS_DOWN].isValid = true;
                localPixelTriadBuffer[CROSS_DOWN].nextPixelGeneral = triadBuffer + downTriadNext.row * size * 4 + downTriadNext.col * 4;
                Pixel downTriadPrev = scan(imageBuffer, size, currentPixel, down, true);
                localPixelTriadBuffer[CROSS_DOWN].prevPixelGeneral = triadBuffer + downTriadPrev.row * size * 4 + downTriadPrev.col * 4;
            }
        }
    }
}

BorderList getStartBorders(Triad* triadBuffer, const jbyte* const imageBuffer, const int size, const int startRow, const int endRow, const int startCol, const int endCol) {
    BorderList borderList {new std::list<Border>(), new std::list<Border>()};
    for (int row = startRow; row < endRow; row++) {
        for (int col = startCol; col < endCol; col++) {
            for (int triad = 0; triad < 4; triad++) {
                Triad& localTriad = triadBuffer[row * size * 4 + col * 4 + triad];
                if (localTriad.isValid && !localTriad.hasBeenFollowed) {
                    if (localTriad.nextPixelGeneral == nullptr) {
                        // Single-pixel border
                        borderList.closedBorders->emplace_back(Border {&localTriad, &localTriad});
                    } else {
                        // Follow border, linking up Triad pointers as we go
                        localTriad.hasBeenFollowed = true;
                        Border currentBorder = {&localTriad};
                        Triad* currentTriad = &localTriad;
                        int currentTriadIndex = triad;
                        while (true) {
                            Triad* nextTriad = currentTriad->nextPixelGeneral;
                            int nextTriadIndex = 0;
                            while (true) {
                                if (nextTriad->isValid && nextTriad->prevPixelGeneral + currentTriadIndex == currentTriad) {
                                    // nextTriad points to the next pixel in the border we're currently following
                                    currentTriad->nextPixelSpecific = nextTriad;
                                    nextTriad->prevPixelSpecific = currentTriad;
                                    break;
                                } else {
                                    nextTriad++;
                                    nextTriadIndex++;
                                }
                            }
                            if (nextTriad == &localTriad) {
                                // We're back to the start!
                                currentBorder.endTriad = currentTriad;
                                borderList.closedBorders->push_back(currentBorder);
                                // Don't even need to set the iterators because they won't be used
                                break;
                            } else if (nextTriad->currentPixel.row < startRow
                                    || nextTriad->currentPixel.row >= endRow
                                    || nextTriad->currentPixel.col < startCol
                                    || nextTriad->currentPixel.col >= endCol) {
                                // Border goes out of this section. Follow it the opposite direction.
                                currentBorder.endTriad = currentTriad;
                                // Follow border, linking up Triad pointers as we go
                                localTriad.hasBeenFollowed = true;
                                currentTriad = &localTriad;
                                currentTriadIndex = triad;
                                while (true) {
                                    Triad* prevTriad = currentTriad->prevPixelGeneral;
                                    int prevTriadIndex = 0;
                                    while (true) {
                                        if (prevTriad->isValid && prevTriad->nextPixelGeneral + currentTriadIndex == currentTriad) {
                                            // nextTriad points to the next pixel in the border we're currently following
                                            currentTriad->prevPixelSpecific = prevTriad;
                                            prevTriad->nextPixelSpecific = currentTriad;
                                            break;
                                        } else {
                                            prevTriad++;
                                            prevTriadIndex++;
                                        }
                                    }
                                    // Impossible to close the loop if the other end has already left the section
                                    if (prevTriad->currentPixel.row < startRow
                                               || prevTriad->currentPixel.row >= endRow
                                               || prevTriad->currentPixel.col < startCol
                                               || prevTriad->currentPixel.col >= endCol) {
                                        // We found the other end of the border. Add it to the list
                                        currentBorder.startTriad = currentTriad;
                                        auto borderIter = borderList.openBorders->insert(borderList.openBorders->end(), currentBorder);
                                        currentBorder.startTriad->border = borderIter;
                                        currentBorder.endTriad->border = borderIter;
                                        break;
                                    } else {
                                        prevTriad->hasBeenFollowed = true;
                                        currentTriad = prevTriad;
                                        currentTriadIndex = prevTriadIndex;
                                    }
                                }
                                break;
                            } else {
                                nextTriad->hasBeenFollowed = true;
                                currentTriad = nextTriad;
                                currentTriadIndex = nextTriadIndex;
                            }
                        }
                    }
                }
            }
        }
    }
    return borderList;
}

void joinBorders(BorderList list1, BorderList list2, const int startRow, const int endRow, const int startCol, const int endCol, bool isJoiningHorizontally) {
    list1.closedBorders->splice(list1.closedBorders->end(), *list2.closedBorders);
    list1.openBorders->splice(list1.openBorders->end(), *list2.openBorders);
    for (auto borderIter = list1.openBorders->begin(); borderIter != list1.openBorders->end();) {
        if (isJoiningHorizontally) {
            int midCol = (startCol + endCol) / 2;
            if ((borderIter->endTriad->currentPixel.col == midCol - 1 && borderIter->endTriad->nextPixelSpecific->currentPixel.col == midCol
                 || borderIter->endTriad->currentPixel.col == midCol && borderIter->endTriad->nextPixelSpecific->currentPixel.col == midCol - 1)
                && (borderIter->endTriad->currentPixel.row < endRow && borderIter->endTriad->currentPixel.row >= startRow
                    && borderIter->endTriad->nextPixelSpecific->currentPixel.row < endRow && borderIter->endTriad->nextPixelSpecific->currentPixel.row >= startRow)) {
                // Both parts of the border are in the current region. We can join them, no further questions asked
                auto nextPartOfBorderIter = borderIter->endTriad->nextPixelSpecific->border;
                borderIter->endTriad = nextPartOfBorderIter->endTriad;
                borderIter->endTriad->border = borderIter;
                list1.openBorders->erase(nextPartOfBorderIter);
                // Check if the border is now closed
                if (borderIter->endTriad->pointsTo(borderIter->startTriad)) {
                    auto nextBorder = std::next(borderIter);
                    list1.closedBorders->splice(list1.closedBorders->end(), *list1.openBorders, borderIter);
                    borderIter = nextBorder;
                }
            } else {
                borderIter++;
            }
        } else {
            int midRow = (startRow + endRow) / 2;
            if ((borderIter->endTriad->currentPixel.row == midRow - 1 && borderIter->endTriad->nextPixelSpecific->currentPixel.row == midRow
                || borderIter->endTriad->currentPixel.row == midRow && borderIter->endTriad->nextPixelSpecific->currentPixel.row == midRow - 1)
                && (borderIter->endTriad->currentPixel.col < endCol && borderIter->endTriad->currentPixel.col >= startCol
                && borderIter->endTriad->nextPixelSpecific->currentPixel.col < endCol && borderIter->endTriad->nextPixelSpecific->currentPixel.col >= startCol)) {
                // Both parts of the border are in the current region. We can join them, no further questions asked
                auto nextPartOfBorderIter = borderIter->endTriad->nextPixelSpecific->border;
                borderIter->endTriad = nextPartOfBorderIter->endTriad;
                borderIter->endTriad->border = borderIter;
                list1.openBorders->erase(nextPartOfBorderIter);
                // Check if the border is now closed
                if (borderIter->endTriad->pointsTo(borderIter->startTriad)) {
                    auto nextBorder = std::next(borderIter);
                    list1.closedBorders->splice(list1.closedBorders->end(), *list1.openBorders, borderIter);
                    borderIter = nextBorder;
                }
            } else {
                borderIter++;
            }
        }
    }
}

#define START_BLOCK_SIZE 4

BorderList findBordersInRectangle(jbyte* imageBuffer, int size) {
    const int startingNumBlocks = size / START_BLOCK_SIZE;
    // 3-D array of dimensions size × size × 4,
    // where the first index is the row, the second index is the column,
    // and the third index is the cross-pixel corresponding to this triad.
    auto* triadBuffer = new Triad[size * size * 4]();
    connectTriads(triadBuffer, imageBuffer, size);
    BorderList joinedBorders[startingNumBlocks][startingNumBlocks];
    // Fill starting blocks with border lists
    for (int i = 0; i < startingNumBlocks; i++) {
        for (int j = 0; j < startingNumBlocks; j++) {
            joinedBorders[i][j] = getStartBorders(
                    triadBuffer,
                    imageBuffer,
                    size,
                    i * START_BLOCK_SIZE,
                    (i + 1) * START_BLOCK_SIZE,
                    j * START_BLOCK_SIZE,
                    (j + 1) * START_BLOCK_SIZE
                    );
        }
    }
    bool willJoinHorizontally = true;
    int currentBlockWidth = START_BLOCK_SIZE;
    int currentBlockHeight = START_BLOCK_SIZE;
    int horizontalStride = 1;
    int verticalStride = 1;
    int currentNumHorizontalBlocks = startingNumBlocks;
    int currentNumVerticalBlocks = startingNumBlocks;
    while (currentBlockWidth < size || currentBlockHeight < size) {
        if (willJoinHorizontally) {
            for (int i = 0; i < startingNumBlocks; i += verticalStride) {
                for (int j = 0; j < startingNumBlocks; j += horizontalStride * 2) {
                    joinBorders(
                            joinedBorders[i][j],
                            joinedBorders[i][j + horizontalStride],
                            i * START_BLOCK_SIZE,
                            (i + verticalStride) * START_BLOCK_SIZE,
                            j * START_BLOCK_SIZE,
                            (j + horizontalStride * 2) * START_BLOCK_SIZE,
                            true
                            );
                    assert(joinedBorders[i][j + horizontalStride].closedBorders->empty());
                    delete joinedBorders[i][j + horizontalStride].closedBorders;
                    assert(joinedBorders[i][j + horizontalStride].closedBorders->empty());
                    delete joinedBorders[i][j + horizontalStride].openBorders;
                }
            }
            currentBlockWidth *= 2;
            horizontalStride *= 2;
            currentNumHorizontalBlocks /= 2;
            willJoinHorizontally = false;
        } else {
            for (int i = 0; i < startingNumBlocks; i += verticalStride * 2) {
                for (int j = 0; j < startingNumBlocks; j += horizontalStride) {
                    joinBorders(
                            joinedBorders[i][j],
                            joinedBorders[i + verticalStride][j],
                            i * START_BLOCK_SIZE,
                            (i + verticalStride * 2) * START_BLOCK_SIZE,
                            j * START_BLOCK_SIZE,
                            (j + horizontalStride) * START_BLOCK_SIZE,
                            false
                            );
                    assert(joinedBorders[i + verticalStride][j].closedBorders->empty());
                    delete joinedBorders[i + verticalStride][j].closedBorders;
                    assert(joinedBorders[i + verticalStride][j].closedBorders->empty());
                    delete joinedBorders[i + verticalStride][j].openBorders;
                }
            }
            currentBlockHeight *= 2;
            verticalStride *= 2;
            currentNumVerticalBlocks /= 2;
            willJoinHorizontally = true;
        }
    }
    return joinedBorders[0][0];
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

        for(Triad* triad = edge.startTriad; true; triad = triad->nextPixelSpecific) {
            // Now, we have object created by Pixel(i, i)
            int row = triad->currentPixel.row;
            int col = triad->currentPixel.col;
            jobject pixelValue = env->NewObject(pixelClass, pixelConstructorID, row, col);
            if(pixelValue == nullptr) {
                return nullptr;
            }

            env->CallBooleanMethod(innerVector, addMethodID, pixelValue);
            if (triad == edge.endTriad) {
                break;
            }
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
Java_com_ajp_ee451finalproject_EdgeDetectionActivity_garciaMollaEdgeFindEfficient(JNIEnv *env, jobject thiz,
                                                                         jbyteArray image,
                                                                         jint width, jint height) {
    assert(width == height);
    jbyte* imageBuffer = env->GetByteArrayElements(image, nullptr);
    struct timespec startTime{};
    clock_gettime(CLOCK_REALTIME, &startTime);
    BorderList borderList = findBordersInRectangle(imageBuffer, width);
    struct timespec endTime{};
    clock_gettime(CLOCK_REALTIME, &endTime);
    double timeMillis = (endTime.tv_sec - startTime.tv_sec + (endTime.tv_nsec - startTime.tv_nsec) / 1000000000.0) * 1000.0;
    assert(borderList.openBorders->empty());
    env->ReleaseByteArrayElements(image, imageBuffer, JNI_ABORT);
    jobject retVal = getJavaEdgeListFromBorderList(env, borderList, timeMillis);
    delete borderList.closedBorders;
    delete borderList.openBorders;
    return retVal;
}