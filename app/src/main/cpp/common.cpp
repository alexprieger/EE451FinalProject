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
#ifdef CONNECTIVITY_8
            case upleft:
                if (imageBuffer[(centerPixel.row - 1) * width + centerPixel.col - 1] != 0) {
                    return {{centerPixel.row - 1, centerPixel.col - 1}, upleft};
                }
                break;
            case upright:
                if (imageBuffer[(centerPixel.row - 1) * width + centerPixel.col + 1] != 0) {
                    return {{centerPixel.row - 1, centerPixel.col + 1}, upright};
                }
                break;
            case downright:
                if (imageBuffer[(centerPixel.row + 1) * width + centerPixel.col + 1] != 0) {
                    return {{centerPixel.row + 1, centerPixel.col + 1}, downright};
                }
                break;
            case downleft:
                if (imageBuffer[(centerPixel.row + 1) * width + centerPixel.col - 1] != 0) {
                    return {{centerPixel.row + 1, centerPixel.col - 1}, downleft};
                }
                break;
#endif
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
        nextDirection = nextCounterClockWiseDirection(nextDirection);
    }
}

jobject getJavaEdgeListFromEdgeList(JNIEnv *env, const std::vector<std::vector<Pixel>>& edgeList, double time) {
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

    jobject result = env->NewObject(edgeDetectionResultClass, edgeDetectionResultConstructorID, outerVector, time);

    env->DeleteLocalRef(edgeDetectionResultClass);
    env->DeleteLocalRef(vectorClass);
    env->DeleteLocalRef(pixelClass);

    return result;
}