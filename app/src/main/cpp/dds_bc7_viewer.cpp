#include <jni.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <limits>

namespace bc7
{
	bool decodeRGBA8(const uint8_t*, size_t, uint32_t, uint32_t, uint32_t, std::vector<uint8_t>&);
}

namespace
{
	uint32_t read32(const uint8_t* p)
	{
		return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
	}
}

// DDS with DX10 header, DXGI_FORMAT_BC7_UNORM (98) or BC7_UNORM_SRGB (99).
// Return ARGB_8888 Java int pixels, or null for unsupported/malformed files.
extern "C" JNIEXPORT jintArray JNICALL
Java_com_example_vulkanwrapper_MainActivity_decodeDdsBC7(JNIEnv* env, jclass,
	jbyteArray file, jintArray dimensions)
{
	if (!file || !dimensions || env->GetArrayLength(dimensions) < 2)
	{
		return nullptr;
	}
	jsize length = env->GetArrayLength(file);
	if (length < 148)
	{
		return nullptr;
	}
	jbyte* data = env->GetByteArrayElements(file, nullptr);
	if (!data)
	{
		return nullptr;
	}
	const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
	bool valid = std::memcmp(p, "DDS ", 4) == 0 &&
		read32(p + 4) == 124 &&
		read32(p + 76) == 32 &&
		std::memcmp(p + 84, "DX10", 4) == 0;
	uint32_t height = read32(p + 12);
	uint32_t width = read32(p + 16);
	uint32_t dxgiFormat = read32(p + 128);
	uint32_t dimension = read32(p + 132);
	uint32_t arraySize = read32(p + 140);
	uint64_t pixels = uint64_t(width) * height;
	uint64_t blocks = ((uint64_t(width) + 3) / 4) * ((uint64_t(height) + 3) / 4);
	valid = valid && (dxgiFormat == 98 || dxgiFormat == 99) &&
		dimension == 3 && arraySize == 1 &&
		width > 0 && height > 0 && width <= 8192 && height <= 8192 &&
		pixels <= static_cast<uint64_t>(std::numeric_limits<jsize>::max()) &&
		blocks <= (uint64_t(length) - 148) / 16;
	std::vector<uint8_t> rgba;
	if (valid)
	{
		valid = bc7::decodeRGBA8(p + 148, size_t(length) - 148,
			width, height, 0, rgba);
	}
	env->ReleaseByteArrayElements(file, data, JNI_ABORT);
	if (!valid)
	{
		return nullptr;
	}
	std::vector<jint> argb(static_cast<size_t>(pixels));
	for (size_t i = 0; i < argb.size(); ++i)
	{
		size_t k = i * 4;
		argb[i] = static_cast<jint>((uint32_t(rgba[k + 3]) << 24) |
			(uint32_t(rgba[k]) << 16) |
			(uint32_t(rgba[k + 1]) << 8) |
			uint32_t(rgba[k + 2]));
	}
	jintArray result = env->NewIntArray(static_cast<jsize>(pixels));
	if (!result)
	{
		return nullptr;
	}
	env->SetIntArrayRegion(result, 0, static_cast<jsize>(pixels), argb.data());
	jint dims[2] = {static_cast<jint>(width), static_cast<jint>(height)};
	env->SetIntArrayRegion(dimensions, 0, 2, dims);
	return result;
}
