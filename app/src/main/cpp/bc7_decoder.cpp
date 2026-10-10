#include <jni.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <limits>
#include <vector>
#include <string>
#define BCDEC_IMPLEMENTATION
#include "third_party/bcdec.h"

// BC7 uses 16 bytes per 4x4 texel block. Output: RGBA8, tightly packed.
// rowPitchBytes is the number of bytes between compressed rows (0 = tightly packed).
// No Vulkan handles are virtualized by this helper.
namespace bc7 {
bool decodeRGBA8(const uint8_t* compressed, size_t compressedBytes,
                 uint32_t width, uint32_t height, uint32_t rowPitchBytes,
                 std::vector<uint8_t>& rgba) {
    if (!compressed || !width || !height) return false;
    const uint64_t blocksX = (static_cast<uint64_t>(width) + 3) / 4;
    const uint64_t blocksY = (static_cast<uint64_t>(height) + 3) / 4;
    const uint64_t tightPitch = blocksX * BCDEC_BC7_BLOCK_SIZE;
    const uint64_t srcPitch = rowPitchBytes ? rowPitchBytes : tightPitch;
    if (srcPitch < tightPitch) return false;
    const uint64_t srcNeeded = (blocksY - 1) * srcPitch + tightPitch;
    const uint64_t dstNeeded = static_cast<uint64_t>(width) * height * 4;
    if (srcNeeded > compressedBytes || dstNeeded > SIZE_MAX ||
        srcPitch > SIZE_MAX || dstNeeded > rgba.max_size()) return false;

    std::vector<uint8_t> output(static_cast<size_t>(dstNeeded));
    uint8_t block[4 * 4 * 4];
    for (uint64_t by = 0; by < blocksY; ++by) {
        for (uint64_t bx = 0; bx < blocksX; ++bx) {
            const uint8_t* src = compressed + static_cast<size_t>(by * srcPitch + bx * 16);
            bcdec_bc7(src, block, 4 * 4);
            for (uint32_t y = 0; y < 4; ++y) {
                uint64_t py = by * 4 + y;
                if (py >= height) break;
                for (uint32_t x = 0; x < 4; ++x) {
                    uint64_t px = bx * 4 + x;
                    if (px >= width) break;
                    size_t dstIndex = static_cast<size_t>((py * width + px) * 4);
                    std::memcpy(output.data() + dstIndex, block + (y * 4 + x) * 4, 4);
                }
            }
        }
    }
    rgba.swap(output);
    return true;
}
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_vulkanwrapper_MainActivity_testBC7(JNIEnv* env, jclass) {
    // Mode 6 (bit 6 set): syntactically valid BC7 block, all other fields zero.
    const uint8_t sample[16] = { 0x40 };
    std::vector<uint8_t> rgba;
    bool ok = bc7::decodeRGBA8(sample, sizeof(sample), 4, 4, 0, rgba);
    if (!ok || rgba.size() != 64) return env->NewStringUTF("BC7: FALHOU");
    char message[160];
    std::snprintf(message, sizeof(message),
                  "BC7 CPU: OK\\nBloco: 16 bytes -> 64 bytes RGBA8\\nPixel 0: %u,%u,%u,%u",
                  rgba[0], rgba[1], rgba[2], rgba[3]);
    return env->NewStringUTF(message);
}
