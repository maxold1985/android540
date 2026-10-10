#include <vulkan/vulkan.h>
#include <android/log.h>
#include <mutex>
#include <unordered_map>
#include <cstdint>
#include <cstring>

// Explicit opt-in device dispatch. This is NOT a loader layer.
// The caller must pass functions obtained from the real device dispatch table.
// Image handles stay real Vulkan handles. No BC7 format substitution is attempted.
namespace imagehook {
struct Dispatch {
    PFN_vkCreateImage createImage = nullptr;
    PFN_vkDestroyImage destroyImage = nullptr;
    PFN_vkCmdCopyBufferToImage copyBufferToImage = nullptr;
};
struct ImageInfo {
    VkFormat format;
    VkExtent3D extent;
    uint32_t mipLevels;
    uint32_t arrayLayers;
};
static std::mutex lock;
static std::unordered_map<uint64_t, ImageInfo> images;

static uint64_t key(VkImage image) {
    // VkImage is a non-dispatchable handle; Vulkan headers may define it as
    // a pointer or uint64_t depending on architecture.
    uint64_t value = 0;
    static_assert(sizeof(image) <= sizeof(value), "Unexpected VkImage size");
    std::memcpy(&value, &image, sizeof(image));
    return value;
}

bool isBC7(VkFormat f) {
    return f == VK_FORMAT_BC7_UNORM_BLOCK || f == VK_FORMAT_BC7_SRGB_BLOCK;
}

VkResult createImage(const Dispatch& d, VkDevice device,
                     const VkImageCreateInfo* info,
                     const VkAllocationCallbacks* allocator, VkImage* out) {
    if (!d.createImage || !info || !out) return VK_ERROR_INITIALIZATION_FAILED;
    if (isBC7(info->format)) {
        __android_log_print(ANDROID_LOG_WARN, "VulkanWrapper",
            "BC7 solicitado: %ux%u mips=%u; conversao automatica ainda indisponivel",
            info->extent.width, info->extent.height, info->mipLevels);
        // Do not claim BC7 support or silently create an incompatible RGBA image.
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
    VkResult result = d.createImage(device, info, allocator, out);
    if (result == VK_SUCCESS) {
        std::lock_guard<std::mutex> guard(lock);
        images[key(*out)] = {info->format, info->extent, info->mipLevels, info->arrayLayers};
        __android_log_print(ANDROID_LOG_INFO, "VulkanWrapper",
            "vkCreateImage: format=%d size=%ux%u", (int)info->format,
            info->extent.width, info->extent.height);
    }
    return result;
}

void destroyImage(const Dispatch& d, VkDevice device, VkImage image,
                  const VkAllocationCallbacks* allocator) {
    if (!d.destroyImage) return;
    {
        std::lock_guard<std::mutex> guard(lock);
        images.erase(key(image));
    }
    d.destroyImage(device, image, allocator);
}

bool copyBufferToImage(const Dispatch& d, VkCommandBuffer cmd, VkBuffer src,
                       VkImage dst, VkImageLayout layout,
                       uint32_t count, const VkBufferImageCopy* regions) {
    if (!d.copyBufferToImage || (count && !regions)) return false;
    {
        std::lock_guard<std::mutex> guard(lock);
        auto it = images.find(key(dst));
        if (it != images.end() && isBC7(it->second.format)) {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanWrapper",
                "Upload BC7 bloqueado: exige decode + staging RGBA8");
            return false;
        }
    }
    __android_log_print(ANDROID_LOG_INFO, "VulkanWrapper",
        "vkCmdCopyBufferToImage: regions=%u", count);
    d.copyBufferToImage(cmd, src, dst, layout, count, regions);
    return true;
}

Dispatch resolve(PFN_vkGetDeviceProcAddr getDeviceProcAddr, VkDevice device) {
    Dispatch d{};
    if (!getDeviceProcAddr || !device) return d;
    d.createImage = reinterpret_cast<PFN_vkCreateImage>(
        getDeviceProcAddr(device, "vkCreateImage"));
    d.destroyImage = reinterpret_cast<PFN_vkDestroyImage>(
        getDeviceProcAddr(device, "vkDestroyImage"));
    d.copyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(
        getDeviceProcAddr(device, "vkCmdCopyBufferToImage"));
    return d;
}
}
