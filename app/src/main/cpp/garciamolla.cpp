#include <jni.h>
#include <vulkan/vulkan.h>
#include <list>

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
    Direction prevDirection;
    Direction nextDirection;
};

std::list<std::list<Triad>> findBordersInRectangle(jbyte* imageBuffer, int startRow, int endRow, int startCol, int endCol) {
    if (endRow - startRow <= 4 && endCol - startCol <= 4) {
        return {};
    }
    // Process block [startRow, endRow) × [startCol, endCol)
    if (endRow - startRow >= endCol - startCol) {
        // More rows than columns, so split vertically
        std::list<std::list<Triad>> list1 = findBordersInRectangle(imageBuffer, startRow, (startRow + endRow) / 2, startCol, endCol);
        std::list<std::list<Triad>> list2 = findBordersInRectangle(imageBuffer, (startRow + endRow) / 2, endRow, startCol, endCol);
        list1.splice(list1.end(), list2);
        return list1;
    }
    // More columns than rows, so split horizontally
    std::list<std::list<Triad>> list1 = findBordersInRectangle(imageBuffer, startRow, endRow, startCol, (startCol + endCol) / 2);
    std::list<std::list<Triad>> list2 = findBordersInRectangle(imageBuffer, startRow, endRow, (startCol + endCol) / 2, endCol);
    list1.splice(list1.end(), list2);
    return list1;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_ajp_ee451finalproject_EdgeDetectionActivity_garciaMollaEdgeFind(JNIEnv *env, jobject thiz,
                                                                         jbyteArray image,
                                                                         jint width, jint height) {
    jbyte* imageBuffer = env->GetByteArrayElements(image, nullptr);
    findBordersInRectangle(imageBuffer, 0, width, 0, height);
    env->ReleaseByteArrayElements(image, imageBuffer, JNI_ABORT);
}