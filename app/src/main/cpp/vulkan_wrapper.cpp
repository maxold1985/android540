#include <jni.h>
#include <vulkan/vulkan.h>
#include <android/log.h>
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <cstdio>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "VulkanWrapper", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VulkanWrapper", __VA_ARGS__)

namespace {
void* library = nullptr;
PFN_vkGetInstanceProcAddr getProcAddr = nullptr;
std::once_flag initFlag;

void initialize() {
    library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!library) { LOGE("dlopen: %s", dlerror()); return; }
    getProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library, "vkGetInstanceProcAddr"));
    LOGI("Loader Android Vulkan: %s", getProcAddr ? "OK" : "FALHOU");
}

VkResult createInstanceIntercepted(const VkInstanceCreateInfo* info,
                                   const VkAllocationCallbacks* allocator,
                                   VkInstance* instance) {
    std::call_once(initFlag, initialize);
    if (!getProcAddr) return VK_ERROR_INITIALIZATION_FAILED;
    auto original = reinterpret_cast<PFN_vkCreateInstance>(
        getProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!original) return VK_ERROR_INITIALIZATION_FAILED;
    LOGI("Interceptando vkCreateInstance");
    VkResult result = original(info, allocator, instance);
    LOGI("vkCreateInstance -> %d", static_cast<int>(result));
    return result;
}
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_vulkanwrapper_MainActivity_runVulkanTest(JNIEnv* env, jclass) {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "Xclipse 540 Vulkan Wrapper Test";
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = createInstanceIntercepted(&info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        std::string msg = "vkCreateInstance falhou: " + std::to_string(result);
        return env->NewStringUTF(msg.c_str());
    }

    auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        getProcAddr(instance, "vkEnumeratePhysicalDevices"));
    auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        getProcAddr(instance, "vkGetPhysicalDeviceProperties"));
    auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(
        getProcAddr(instance, "vkDestroyInstance"));
    std::string msg;
    if (!enumerate || !properties || !destroy) {
        msg = "Funcoes Vulkan nao encontradas";
    } else {
        uint32_t count = 0;
        VkResult r = enumerate(instance, &count, nullptr);
        if (r == VK_SUCCESS && count > 0) {
            VkPhysicalDevice gpu = VK_NULL_HANDLE;
            uint32_t capacity = 1;
            r = enumerate(instance, &capacity, &gpu);
            if ((r == VK_SUCCESS || r == VK_INCOMPLETE) && capacity > 0) {
                VkPhysicalDeviceProperties p{};
                properties(gpu, &p);
                char buffer[512];
                std::snprintf(buffer, sizeof(buffer),
                    "GPU: %s\nVulkan: %u.%u.%u\nVendor: 0x%04x\nDevices: %u\nInterceptacao: OK",
                    p.deviceName, VK_VERSION_MAJOR(p.apiVersion), VK_VERSION_MINOR(p.apiVersion),
                    VK_VERSION_PATCH(p.apiVersion), p.vendorID, count);
                msg = buffer;
                LOGI("%s", buffer);
            }
        }
        if (msg.empty()) msg = "Nao foi possivel enumerar GPU Vulkan";
    }
    if (destroy) destroy(instance, nullptr);
    return env->NewStringUTF(msg.c_str());
}
