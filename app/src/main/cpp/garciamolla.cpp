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
    int prevPixelGeneral;
    // If isValid is true and nextPixelGeneral is -1, this pixel is isolated
    int nextPixelGeneral; // Direction points towards currentPixel
    // The "general" pointers point to the next triad in the border
    // Initialized by getStartBorders
    int prevPixelSpecific;
    int nextPixelSpecific;
    std::list<Border>::iterator border;
    // False if the corresponding cross pixel is not black, or if this triad would be duplicated by another
    bool isValid;
    bool hasBeenFollowed;
};

bool pointsTo(Triad* triadBuffer, int fromTriad, int toTriad) {
    return triadBuffer[fromTriad].nextPixelSpecific == toTriad && triadBuffer[toTriad].prevPixelSpecific == fromTriad;
}

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

struct ConnectTriadsInput {
    Triad* triadBuffer;
    jbyte* imageBuffer;
    int size;
    int startRow;
    int endRow;
    int startCol;
    int endCol;
};

void connectTriads(ConnectTriadsInput* input) {
    Triad* triadBuffer = input->triadBuffer;
    jbyte* imageBuffer = input->imageBuffer;
    const int size = input->size;
    const int startRow = input->startRow;
    const int endRow = input->endRow;
    const int startCol = input->startCol;
    const int endCol = input->endCol;
    for (int currentPixelRow = startRow; currentPixelRow < endRow; currentPixelRow++) {
        for (int currentPixelColumn = startCol; currentPixelColumn < endCol; currentPixelColumn++) {
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
                    localPixelTriadBuffer[CROSS_LEFT].nextPixelGeneral = -1;
                    localPixelTriadBuffer[CROSS_UP].isValid = false;
                    localPixelTriadBuffer[CROSS_RIGHT].isValid = false;
                    localPixelTriadBuffer[CROSS_DOWN].isValid = false;
                    continue;
                } else {
                    localPixelTriadBuffer[CROSS_LEFT].isValid = true;
                    localPixelTriadBuffer[CROSS_LEFT].nextPixelGeneral = leftTriadNext.row * size * 4 + leftTriadNext.col * 4;
                    Pixel leftTriadPrev = scan(imageBuffer, size, currentPixel, left, true);
                    localPixelTriadBuffer[CROSS_LEFT].prevPixelGeneral = leftTriadPrev.row * size * 4 + leftTriadPrev.col * 4;
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
                localPixelTriadBuffer[CROSS_UP].nextPixelGeneral = upTriadNext.row * size * 4 + upTriadNext.col * 4;
                Pixel leftTriadPrev = scan(imageBuffer, size, currentPixel, up, true);
                localPixelTriadBuffer[CROSS_UP].prevPixelGeneral = leftTriadPrev.row * size * 4 + leftTriadPrev.col * 4;
            }
            // Get right triad
            if (imageBuffer[currentPixelRow * size + currentPixelColumn + 1] != 0 || imageBuffer[currentPixelRow * size + currentPixelColumn] & RIGHT_MASK) {
                // No right triad, or else it's the same as the left or up triad
                localPixelTriadBuffer[CROSS_RIGHT].isValid = false;
            } else {
                Pixel rightTriadNext = scan(imageBuffer, size, currentPixel, right, false);
                localPixelTriadBuffer[CROSS_RIGHT].isValid = true;
                localPixelTriadBuffer[CROSS_RIGHT].nextPixelGeneral = rightTriadNext.row * size * 4 + rightTriadNext.col * 4;
                Pixel rightTriadPrev = scan(imageBuffer, size, currentPixel, right, true);
                localPixelTriadBuffer[CROSS_RIGHT].prevPixelGeneral = rightTriadPrev.row * size * 4 + rightTriadPrev.col * 4;
            }
            // Get down triad
            if (imageBuffer[(currentPixelRow + 1) * size + currentPixelColumn] != 0 || imageBuffer[currentPixelRow * size + currentPixelColumn] & DOWN_MASK) {
                // No down triad, or else it's the same as the left, up, or right triad
                localPixelTriadBuffer[CROSS_DOWN].isValid = false;
            } else {
                Pixel downTriadNext = scan(imageBuffer, size, currentPixel, down, false);
                localPixelTriadBuffer[CROSS_DOWN].isValid = true;
                localPixelTriadBuffer[CROSS_DOWN].nextPixelGeneral = downTriadNext.row * size * 4 + downTriadNext.col * 4;
                Pixel downTriadPrev = scan(imageBuffer, size, currentPixel, down, true);
                localPixelTriadBuffer[CROSS_DOWN].prevPixelGeneral = downTriadPrev.row * size * 4 + downTriadPrev.col * 4;
            }
        }
    }
}

struct GetStartBordersInput {
    Triad* triadBuffer;
    jbyte* imageBuffer;
    int size;
    int startRow;
    int endRow;
    int startCol;
    int endCol;
    BorderList result;
};

void getStartBorders(GetStartBordersInput* getStartBordersInput) {
    Triad* triadBuffer = getStartBordersInput->triadBuffer;
    const jbyte* const imageBuffer = getStartBordersInput->imageBuffer;
    const int size = getStartBordersInput->size;
    const int startRow = getStartBordersInput->startRow;
    const int endRow = getStartBordersInput->endRow;
    const int startCol = getStartBordersInput->startCol;
    const int endCol = getStartBordersInput->endCol;
    BorderList borderList {new std::list<Border>(), new std::list<Border>()};
    for (int row = startRow; row < endRow; row++) {
        for (int col = startCol; col < endCol; col++) {
            for (int triad = 0; triad < 4; triad++) {
                int localTriad = row * size * 4 + col * 4 + triad;
                if (triadBuffer[localTriad].isValid && !triadBuffer[localTriad].hasBeenFollowed) {
                    if (triadBuffer[localTriad].nextPixelGeneral == -1) {
                        // Single-pixel border
                        borderList.closedBorders->emplace_back(Border {triadBuffer + localTriad, triadBuffer + localTriad});
                    } else {
                        // Follow border, linking up Triad pointers as we go
                        triadBuffer[localTriad].hasBeenFollowed = true;
                        Border currentBorder = {triadBuffer + localTriad};
                        int currentTriad = row * size * 4 + col * 4 + triad;
                        int currentTriadIndex = triad;
                        while (true) {
                            int nextTriad = triadBuffer[currentTriad].nextPixelGeneral;
                            int nextTriadIndex = 0;
                            while (true) {
                                if (triadBuffer[nextTriad].isValid && triadBuffer[nextTriad].prevPixelGeneral + currentTriadIndex == currentTriad) {
                                    // nextTriad points to the next pixel in the border we're currently following
                                    triadBuffer[currentTriad].nextPixelSpecific = nextTriad;
                                    triadBuffer[nextTriad].prevPixelSpecific = currentTriad;
                                    break;
                                } else {
                                    nextTriad++;
                                    nextTriadIndex++;
                                }
                            }
                            if (nextTriad == localTriad) {
                                // We're back to the start!
                                currentBorder.endTriad = triadBuffer + currentTriad;
                                borderList.closedBorders->push_back(currentBorder);
                                // Don't even need to set the iterators because they won't be used
                                break;
                            } else if (triadBuffer[nextTriad].currentPixel.row < startRow
                                       || triadBuffer[nextTriad].currentPixel.row >= endRow
                                       || triadBuffer[nextTriad].currentPixel.col < startCol
                                       || triadBuffer[nextTriad].currentPixel.col >= endCol) {
                                // Border goes out of this section. Follow it the opposite direction.
                                currentBorder.endTriad = triadBuffer + currentTriad;
                                // Follow border, linking up Triad pointers as we go
                                triadBuffer[localTriad].hasBeenFollowed = true;
                                currentTriad = localTriad;
                                currentTriadIndex = triad;
                                while (true) {
                                    int prevTriad = triadBuffer[currentTriad].prevPixelGeneral;
                                    int prevTriadIndex = 0;
                                    while (true) {
                                        if (triadBuffer[prevTriad].isValid && triadBuffer[prevTriad].nextPixelGeneral + currentTriadIndex == currentTriad) {
                                            // nextTriad points to the next pixel in the border we're currently following
                                            triadBuffer[currentTriad].prevPixelSpecific = prevTriad;
                                            triadBuffer[prevTriad].nextPixelSpecific = currentTriad;
                                            break;
                                        } else {
                                            prevTriad++;
                                            prevTriadIndex++;
                                        }
                                    }
                                    // Impossible to close the loop if the other end has already left the section
                                    if (triadBuffer[prevTriad].currentPixel.row < startRow
                                        || triadBuffer[prevTriad].currentPixel.row >= endRow
                                        || triadBuffer[prevTriad].currentPixel.col < startCol
                                        || triadBuffer[prevTriad].currentPixel.col >= endCol) {
                                        // We found the other end of the border. Add it to the list
                                        currentBorder.startTriad = triadBuffer + currentTriad;
                                        auto borderIter = borderList.openBorders->insert(borderList.openBorders->end(), currentBorder);
                                        currentBorder.startTriad->border = borderIter;
                                        currentBorder.endTriad->border = borderIter;
                                        break;
                                    } else {
                                        triadBuffer[prevTriad].hasBeenFollowed = true;
                                        currentTriad = prevTriad;
                                        currentTriadIndex = prevTriadIndex;
                                    }
                                }
                                break;
                            } else {
                                triadBuffer[nextTriad].hasBeenFollowed = true;
                                currentTriad = nextTriad;
                                currentTriadIndex = nextTriadIndex;
                            }
                        }
                    }
                }
            }
        }
    }
    getStartBordersInput->result = borderList;
}

struct JoinBordersInput {
    BorderList list1;
    BorderList list2;
    Triad* triadBuffer;
    int startRow;
    int endRow;
    int startCol;
    int endCol;
    bool isJoiningHorizontally;
};

void joinBorders(JoinBordersInput* joinBordersInput) {
    BorderList list1 = joinBordersInput->list1;
    BorderList list2 = joinBordersInput->list2;
    Triad* triadBuffer = joinBordersInput->triadBuffer;
    const int startRow = joinBordersInput->startRow;
    const int endRow = joinBordersInput->endRow;
    const int startCol = joinBordersInput->startCol;
    const int endCol = joinBordersInput->endCol;
    bool isJoiningHorizontally = joinBordersInput->isJoiningHorizontally;
    list1.closedBorders->splice(list1.closedBorders->end(), *list2.closedBorders);
    list1.openBorders->splice(list1.openBorders->end(), *list2.openBorders);
    for (auto borderIter = list1.openBorders->begin(); borderIter != list1.openBorders->end();) {
        if (isJoiningHorizontally) {
            int midCol = (startCol + endCol) / 2;
            if ((borderIter->endTriad->currentPixel.col == midCol - 1 && triadBuffer[borderIter->endTriad->nextPixelSpecific].currentPixel.col == midCol
                 || borderIter->endTriad->currentPixel.col == midCol && triadBuffer[borderIter->endTriad->nextPixelSpecific].currentPixel.col == midCol - 1)
                && (borderIter->endTriad->currentPixel.row < endRow && borderIter->endTriad->currentPixel.row >= startRow
                    && triadBuffer[borderIter->endTriad->nextPixelSpecific].currentPixel.row < endRow && triadBuffer[borderIter->endTriad->nextPixelSpecific].currentPixel.row >= startRow)) {
                // Both parts of the border are in the current region. We can join them, no further questions asked
                auto nextPartOfBorderIter = triadBuffer[borderIter->endTriad->nextPixelSpecific].border;
                borderIter->endTriad = nextPartOfBorderIter->endTriad;
                borderIter->endTriad->border = borderIter;
                list1.openBorders->erase(nextPartOfBorderIter);
                // Check if the border is now closed
                if (pointsTo(triadBuffer, borderIter->endTriad - triadBuffer, borderIter->startTriad - triadBuffer)) {
                    auto nextBorder = std::next(borderIter);
                    list1.closedBorders->splice(list1.closedBorders->end(), *list1.openBorders, borderIter);
                    borderIter = nextBorder;
                }
            } else {
                borderIter++;
            }
        } else {
            int midRow = (startRow + endRow) / 2;
            if ((borderIter->endTriad->currentPixel.row == midRow - 1 && triadBuffer[borderIter->endTriad->nextPixelSpecific].currentPixel.row == midRow
                 || borderIter->endTriad->currentPixel.row == midRow && triadBuffer[borderIter->endTriad->nextPixelSpecific].currentPixel.row == midRow - 1)
                && (borderIter->endTriad->currentPixel.col < endCol && borderIter->endTriad->currentPixel.col >= startCol
                    && triadBuffer[borderIter->endTriad->nextPixelSpecific].currentPixel.col < endCol && triadBuffer[borderIter->endTriad->nextPixelSpecific].currentPixel.col >= startCol)) {
                // Both parts of the border are in the current region. We can join them, no further questions asked
                auto nextPartOfBorderIter = triadBuffer[borderIter->endTriad->nextPixelSpecific].border;
                borderIter->endTriad = nextPartOfBorderIter->endTriad;
                borderIter->endTriad->border = borderIter;
                list1.openBorders->erase(nextPartOfBorderIter);
                // Check if the border is now closed
                if (pointsTo(triadBuffer, borderIter->endTriad - triadBuffer, borderIter->startTriad - triadBuffer)) {
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

struct GarciaMollaResult {
    BorderList result;
    Triad* triadBuffer;
    double timeSetupMillis;
    double timeSuzukiMillis;
    double timeJoinMills;
    double timeTotalMillis;
};

GarciaMollaResult findBordersInRectangle(jbyte* imageBuffer, int size) {
    struct timespec startSetupTime{};
    clock_gettime(CLOCK_REALTIME, &startSetupTime);
    const int startingNumBlocks = size / START_BLOCK_SIZE;
    // 3-D array of dimensions size × size × 4,
    // where the first index is the row, the second index is the column,
    // and the third index is the cross-pixel corresponding to this triad.
    auto* triadBuffer = new Triad[size * size * 4]();
    ConnectTriadsInput connectTriadsInput { triadBuffer, imageBuffer, size, 0, size, 0, size };
    connectTriads(&connectTriadsInput);
    struct timespec endSetupTime{};
    clock_gettime(CLOCK_REALTIME, &endSetupTime);
    BorderList joinedBorders[startingNumBlocks][startingNumBlocks];
    // Fill starting blocks with border lists
    for (int i = 0; i < startingNumBlocks; i++) {
        for (int j = 0; j < startingNumBlocks; j++) {
            GetStartBordersInput getStartBordersInput { triadBuffer, imageBuffer, size, i * START_BLOCK_SIZE, (i + 1) * START_BLOCK_SIZE, j * START_BLOCK_SIZE, (j + 1) * START_BLOCK_SIZE };
            getStartBorders(&getStartBordersInput);
            joinedBorders[i][j] = getStartBordersInput.result;
        }
    }
    struct timespec endSuzukiTime{};
    clock_gettime(CLOCK_REALTIME, &endSuzukiTime);
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
                    JoinBordersInput joinBordersInput { joinedBorders[i][j], joinedBorders[i][j + horizontalStride], triadBuffer, i * START_BLOCK_SIZE, (i + verticalStride) * START_BLOCK_SIZE, j * START_BLOCK_SIZE, (j + horizontalStride * 2) * START_BLOCK_SIZE, true};
                    joinBorders(&joinBordersInput);
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
                    JoinBordersInput joinBordersInput { joinedBorders[i][j], joinedBorders[i + verticalStride][j], triadBuffer, i * START_BLOCK_SIZE, (i + verticalStride * 2) * START_BLOCK_SIZE, j * START_BLOCK_SIZE, (j + horizontalStride) * START_BLOCK_SIZE, false };
                    joinBorders(&joinBordersInput);
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
    struct timespec endJoinTime{};
    clock_gettime(CLOCK_REALTIME, &endJoinTime);
    double timeSetupMillis = (endSetupTime.tv_sec - startSetupTime.tv_sec + (endSetupTime.tv_nsec - startSetupTime.tv_nsec) / 1000000000.0) * 1000.0;
    double timeSuzukiMillis = (endSuzukiTime.tv_sec - endSetupTime.tv_sec + (endSuzukiTime.tv_nsec - endSetupTime.tv_nsec) / 1000000000.0) * 1000.0;
    double timeJoinMillis = (endJoinTime.tv_sec - endSetupTime.tv_sec + (endJoinTime.tv_nsec - endSetupTime.tv_nsec) / 1000000000.0) * 1000.0;
    return {joinedBorders[0][0], triadBuffer, timeSetupMillis, timeSuzukiMillis, timeJoinMillis };
}

GarciaMollaResult findBordersInRectangleParallel(jbyte* imageBuffer, const int size, const int lgSqrtNumThreads) {
    const int numParallelSections = (1 << lgSqrtNumThreads); // The number in one dimension only
    const int parallelBlockSize = size >> lgSqrtNumThreads;
    struct timespec startSetupTime{};
    clock_gettime(CLOCK_REALTIME, &startSetupTime);
    // 3-D array of dimensions size × size × 4,
    // where the first index is the row, the second index is the column,
    // and the third index is the cross-pixel corresponding to this triad.
    auto* triadBuffer = new Triad[size * size * 4]();
    ConnectTriadsInput connectTriadsInput[numParallelSections][numParallelSections];
    pthread_t connectPthreads[numParallelSections][numParallelSections];
    for (int i = 0; i < numParallelSections; i++) {
        for (int j = 0; j < numParallelSections; j++) {
            connectTriadsInput[i][j] = { triadBuffer, imageBuffer, size, i * parallelBlockSize, (i + 1) * parallelBlockSize, j * parallelBlockSize, (j + 1) * parallelBlockSize };
            pthread_create(&(connectPthreads[i][j]), nullptr, reinterpret_cast<void *(*)(void *)>(connectTriads), &(connectTriadsInput[i][j]));
        }
    }
    for (int i = 0; i < numParallelSections; i++) {
        for (int j = 0; j < numParallelSections; j++) {
            pthread_join(connectPthreads[i][j], nullptr);
        }
    }
    struct timespec endSetupTime{};
    clock_gettime(CLOCK_REALTIME, &endSetupTime);
    BorderList joinedBorders[numParallelSections][numParallelSections];
    // Fill starting blocks with border lists
    GetStartBordersInput getStartBordersInput[numParallelSections][numParallelSections];
    pthread_t startPthreads[numParallelSections][numParallelSections];
    for (int i = 0; i < numParallelSections; i++) {
        for (int j = 0; j < numParallelSections; j++) {
            getStartBordersInput[i][j] = { triadBuffer, imageBuffer, size, i * parallelBlockSize, (i + 1) * parallelBlockSize, j * parallelBlockSize, (j + 1) * parallelBlockSize };
            pthread_create(&(startPthreads[i][j]), nullptr, reinterpret_cast<void *(*)(void *)>(getStartBorders), &(getStartBordersInput[i][j]));
        }
    }
    for (int i = 0; i < numParallelSections; i++) {
        for (int j = 0; j < numParallelSections; j++) {
            pthread_join(startPthreads[i][j], nullptr);
            joinedBorders[i][j] = getStartBordersInput[i][j].result;
        }
    }
    struct timespec endSuzukiTime{};
    clock_gettime(CLOCK_REALTIME, &endSuzukiTime);
    bool willJoinHorizontally = true;
    int currentBlockWidth = parallelBlockSize;
    int currentBlockHeight = parallelBlockSize;
    int horizontalStride = 1;
    int verticalStride = 1;
    int currentNumHorizontalBlocks = numParallelSections;
    int currentNumVerticalBlocks = numParallelSections;
    while (currentBlockWidth < size || currentBlockHeight < size) {
        // This section could be parallelized in principal but on the CPU it's so short there's no point
        if (willJoinHorizontally) {
            for (int i = 0; i < numParallelSections; i += verticalStride) {
                for (int j = 0; j < numParallelSections; j += horizontalStride * 2) {
                    JoinBordersInput joinBordersInput { joinedBorders[i][j],joinedBorders[i][j + horizontalStride],triadBuffer, i * parallelBlockSize,(i + verticalStride) * parallelBlockSize,j * parallelBlockSize,(j + horizontalStride * 2) * parallelBlockSize,true };
                    joinBorders(&joinBordersInput);
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
            for (int i = 0; i < numParallelSections; i += verticalStride * 2) {
                for (int j = 0; j < numParallelSections; j += horizontalStride) {
                    JoinBordersInput joinBordersInput {joinedBorders[i][j],joinedBorders[i + verticalStride][j],triadBuffer, i * parallelBlockSize,(i + verticalStride * 2) * parallelBlockSize,j * parallelBlockSize,(j + horizontalStride) * parallelBlockSize,false };
                    joinBorders(&joinBordersInput);
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
    struct timespec endJoinTime{};
    clock_gettime(CLOCK_REALTIME, &endJoinTime);
    double timeSetupMillis = (endSetupTime.tv_sec - startSetupTime.tv_sec + (endSetupTime.tv_nsec - startSetupTime.tv_nsec) / 1000000000.0) * 1000.0;
    double timeSuzukiMillis = (endSuzukiTime.tv_sec - endSetupTime.tv_sec + (endSuzukiTime.tv_nsec - endSetupTime.tv_nsec) / 1000000000.0) * 1000.0;
    double timeJoinMillis = (endJoinTime.tv_sec - endSetupTime.tv_sec + (endJoinTime.tv_nsec - endSetupTime.tv_nsec) / 1000000000.0) * 1000.0;
    return {joinedBorders[0][0], triadBuffer, timeSetupMillis, timeJoinMillis };
}

jobject getJavaEdgeListFromBorderList(JNIEnv *env, const GarciaMollaResult& borderResult) {
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
                                                                  "(Ljava/util/Vector;DDDD)V");
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

    for(auto& edge : *(borderResult.result.closedBorders)) {

        // Inner vector
        jobject innerVector = env->NewObject(vectorClass, vectorConstructorID);
        if(innerVector == nullptr) {
            return nullptr;
        }

        for(Triad* triad = edge.startTriad; true; triad = borderResult.triadBuffer + triad->nextPixelSpecific) {
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

    jobject result = env->NewObject(edgeDetectionResultClass, edgeDetectionResultConstructorID, outerVector, borderResult.timeSetupMillis, borderResult.timeSuzukiMillis, borderResult.timeJoinMills, borderResult.timeTotalMillis);

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
    assert(width == height);
    jbyte* imageBuffer = env->GetByteArrayElements(image, nullptr);
    struct timespec startTime{};
    clock_gettime(CLOCK_REALTIME, &startTime);
    GarciaMollaResult result = findBordersInRectangle(imageBuffer, width);
    struct timespec endTime{};
    clock_gettime(CLOCK_REALTIME, &endTime);
    double timeMillis = (endTime.tv_sec - startTime.tv_sec + (endTime.tv_nsec - startTime.tv_nsec) / 1000000000.0) * 1000.0;
    result.timeTotalMillis = timeMillis;
    assert(result.result.openBorders->empty());
    env->ReleaseByteArrayElements(image, imageBuffer, JNI_ABORT);
    jobject retVal = getJavaEdgeListFromBorderList(env, result);
    delete result.result.closedBorders;
    delete result.result.openBorders;
    delete result.triadBuffer;
    return retVal;
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_ajp_ee451finalproject_EdgeDetectionActivity_garciaMollaEdgeFindParallel(JNIEnv *env, jobject thiz, jbyteArray image, jint width, jint height,
                                                                                 jint lg_num_threads) {
    assert(width == height);
    jbyte* imageBuffer = env->GetByteArrayElements(image, nullptr);
    struct timespec startTime{};
    clock_gettime(CLOCK_REALTIME, &startTime);
    GarciaMollaResult result = findBordersInRectangleParallel(imageBuffer, width, lg_num_threads);
    struct timespec endTime{};
    clock_gettime(CLOCK_REALTIME, &endTime);
    double timeMillis = (endTime.tv_sec - startTime.tv_sec + (endTime.tv_nsec - startTime.tv_nsec) / 1000000000.0) * 1000.0;
    result.timeTotalMillis = timeMillis;
    assert(result.result.openBorders->empty());
    env->ReleaseByteArrayElements(image, imageBuffer, JNI_ABORT);
    jobject retVal = getJavaEdgeListFromBorderList(env, result);
    delete result.result.closedBorders;
    delete result.result.openBorders;
    delete result.triadBuffer;
    return retVal;
}