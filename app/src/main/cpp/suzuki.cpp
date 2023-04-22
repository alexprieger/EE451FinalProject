#include <jni.h>
#include <android/log.h>
#include <vector>
#include <ctime>

#include "common.h"

extern "C"
JNIEXPORT jobject JNICALL
Java_com_ajp_ee451finalproject_EdgeDetectionActivity_suzukiEdgeFind(JNIEnv *env, jobject thiz,
                                                                    jbyteArray image, jint width,
                                                                    jint height) {
    // Image array is in row major order, with 0 representing black and 1 representing white
    jbyte* imageBuffer = env->GetByteArrayElements(image, nullptr);
    struct timespec startTime{};
    clock_gettime(CLOCK_REALTIME, &startTime);
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
    struct timespec endTime{};
    clock_gettime(CLOCK_REALTIME, &endTime);
    env->ReleaseByteArrayElements(image, imageBuffer, JNI_ABORT);
    double timeMillis = (endTime.tv_sec - startTime.tv_sec + (endTime.tv_nsec - startTime.tv_nsec) / 1000000000.0) * 1000.0;
    jobject retVal = getJavaEdgeListFromEdgeList(env, edgeList, timeMillis);
    return retVal;
}