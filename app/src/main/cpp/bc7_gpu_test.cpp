#include <jni.h>
#include <vulkan/vulkan.h>
#include <android/log.h>
#include <dlfcn.h>
#include <cstdint>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>

namespace bc7 {
bool decodeRGBA8(const uint8_t*, size_t, uint32_t, uint32_t, uint32_t,
				 std::vector<uint8_t>&);
}
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "VulkanWrapper", __VA_ARGS__)

// Proof of concept: BC7 CPU -> RGBA8 staging buffer -> Vulkan RGBA8 image.
// No transparent Vulkan interception or DXVK integration is claimed.
extern "C" JNIEXPORT jstring JNICALL
Java_com_example_vulkanwrapper_MainActivity_testBC7GPU(JNIEnv* env, jclass) {
	void* lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
	if (!lib) return env->NewStringUTF("GPU BC7: libvulkan.so indisponivel");
	auto gpa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(lib, "vkGetInstanceProcAddr"));
	if (!gpa) { dlclose(lib);
		return env->NewStringUTF("GPU BC7: vkGetInstanceProcAddr ausente");
		}
	auto ci = reinterpret_cast<PFN_vkCreateInstance>(gpa(VK_NULL_HANDLE, "vkCreateInstance"));
	if (!ci) { dlclose(lib);
		return env->NewStringUTF("GPU BC7: vkCreateInstance ausente");
		}

	VkInstance instance = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory imageMemory = VK_NULL_HANDLE, stagingMemory = VK_NULL_HANDLE;
	VkBuffer staging = VK_NULL_HANDLE;
	VkCommandPool pool = VK_NULL_HANDLE;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	std::string status = "GPU BC7: falha";
	VkResult result = VK_SUCCESS;

	VkInstanceCreateInfo ici{};
		ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	result = ci(&ici, nullptr, &instance);
	if (result != VK_SUCCESS) { dlclose(lib);
		return env->NewStringUTF("GPU BC7: falha ao criar instancia");
		}
	auto ip = [&](const char* n) { return gpa(instance, n);
		};
	auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(ip("vkEnumeratePhysicalDevices"));
	auto families = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(ip("vkGetPhysicalDeviceQueueFamilyProperties"));
	auto memoryProps = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(ip("vkGetPhysicalDeviceMemoryProperties"));
	auto formatProps = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(ip("vkGetPhysicalDeviceFormatProperties"));
	auto createDevice = reinterpret_cast<PFN_vkCreateDevice>(ip("vkCreateDevice"));
	auto destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(ip("vkDestroyInstance"));
	auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>(ip("vkGetDeviceProcAddr"));

	if (!enumerate || !families || !memoryProps || !formatProps || !createDevice || !destroyInstance || !gdpa) {
		status = "GPU BC7: funcoes de instancia ausentes";
	} else {
		do {
			uint32_t count = 0;
			result = enumerate(instance, &count, nullptr);
			if (result != VK_SUCCESS || count == 0) { status = "GPU BC7: sem GPU";
				break;
				}
			std::vector<VkPhysicalDevice> gpus(count);
			result = enumerate(instance, &count, gpus.data());
			if (result != VK_SUCCESS || count == 0) { status = "GPU BC7: erro ao enumerar GPU";
				break;
				}
			VkPhysicalDevice gpu = gpus[0];
			uint32_t familyCount = 0;
			families(gpu, &familyCount, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilies(familyCount);
			families(gpu, &familyCount, queueFamilies.data());
			uint32_t family = familyCount;
			for (uint32_t i = 0; i < familyCount; ++i) {
				if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { family = i;
					break;
					}
			}
			if (family == familyCount) { status = "GPU BC7: sem fila grafica";
				break;
				}
			VkFormatProperties fp{};
			formatProps(gpu, VK_FORMAT_R8G8B8A8_UNORM, &fp);
			if (!(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)) {
				status = "GPU BC7: RGBA8 nao aceita transferencia";
					break;
			}
			float priority = 1.f;
			VkDeviceQueueCreateInfo qci{};
				qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			qci.queueFamilyIndex = family;
				qci.queueCount = 1;
				qci.pQueuePriorities = &priority;
			VkDeviceCreateInfo dci{};
				dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			dci.queueCreateInfoCount = 1;
				dci.pQueueCreateInfos = &qci;
			result = createDevice(gpu, &dci, nullptr, &device);
			if (result != VK_SUCCESS) { status = "GPU BC7: vkCreateDevice falhou";
				break;
				}

			auto dp = [&](const char* n) { return gdpa(device, n);
				};
			auto createImage = reinterpret_cast<PFN_vkCreateImage>(dp("vkCreateImage"));
			auto getImageReq = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(dp("vkGetImageMemoryRequirements"));
			auto allocate = reinterpret_cast<PFN_vkAllocateMemory>(dp("vkAllocateMemory"));
			auto bindImage = reinterpret_cast<PFN_vkBindImageMemory>(dp("vkBindImageMemory"));
			auto createBuffer = reinterpret_cast<PFN_vkCreateBuffer>(dp("vkCreateBuffer"));
			auto getBufferReq = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(dp("vkGetBufferMemoryRequirements"));
			auto bindBuffer = reinterpret_cast<PFN_vkBindBufferMemory>(dp("vkBindBufferMemory"));
			auto map = reinterpret_cast<PFN_vkMapMemory>(dp("vkMapMemory"));
			auto unmap = reinterpret_cast<PFN_vkUnmapMemory>(dp("vkUnmapMemory"));
			auto createPool = reinterpret_cast<PFN_vkCreateCommandPool>(dp("vkCreateCommandPool"));
			auto allocCmd = reinterpret_cast<PFN_vkAllocateCommandBuffers>(dp("vkAllocateCommandBuffers"));
			auto begin = reinterpret_cast<PFN_vkBeginCommandBuffer>(dp("vkBeginCommandBuffer"));
			auto barrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(dp("vkCmdPipelineBarrier"));
			auto copy = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(dp("vkCmdCopyBufferToImage"));
			auto end = reinterpret_cast<PFN_vkEndCommandBuffer>(dp("vkEndCommandBuffer"));
			auto getQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(dp("vkGetDeviceQueue"));
			auto submit = reinterpret_cast<PFN_vkQueueSubmit>(dp("vkQueueSubmit"));
			auto wait = reinterpret_cast<PFN_vkQueueWaitIdle>(dp("vkQueueWaitIdle"));
			if (!createImage || !getImageReq || !allocate || !bindImage || !createBuffer ||
				!getBufferReq || !bindBuffer || !map || !unmap || !createPool ||
				!allocCmd || !begin || !barrier || !copy || !end || !getQueue || !submit || !wait) {
				status = "GPU BC7: funcoes de dispositivo ausentes";
					break;
			}
			VkPhysicalDeviceMemoryProperties mp{};
				memoryProps(gpu, &mp);
			auto chooseMemory = [&](uint32_t bits, VkMemoryPropertyFlags flags) -> uint32_t {
				for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
					if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & flags) == flags) return i;
				return UINT32_MAX;
			};
			// A valid BC7 mode-6 block, decoded to a 4x4 RGBA8 texture.
			uint8_t bc7Block[16] = {0x40};
			std::vector<uint8_t> rgba;
			if (!bc7::decodeRGBA8(bc7Block, sizeof(bc7Block), 4, 4, 0, rgba)) {
				status = "GPU BC7: falha na decodificacao";
					break;
			}
			VkImageCreateInfo img{};
				img.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			img.imageType = VK_IMAGE_TYPE_2D;
				img.format = VK_FORMAT_R8G8B8A8_UNORM;
			img.extent = {4,4,1};
				img.mipLevels = 1;
				img.arrayLayers = 1;
			img.samples = VK_SAMPLE_COUNT_1_BIT;
				img.tiling = VK_IMAGE_TILING_OPTIMAL;
			img.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			img.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			img.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			result = createImage(device, &img, nullptr, &image);
			if (result != VK_SUCCESS) { status = "GPU BC7: vkCreateImage RGBA8 falhou";
				break;
				}
			VkMemoryRequirements ir{};
				getImageReq(device, image, &ir);
			uint32_t im = chooseMemory(ir.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			if (im == UINT32_MAX) im = chooseMemory(ir.memoryTypeBits, 0);
			if (im == UINT32_MAX) { status = "GPU BC7: memoria de imagem indisponivel";
				break;
				}
			VkMemoryAllocateInfo mai{};
				mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			mai.allocationSize = ir.size;
				mai.memoryTypeIndex = im;
			result = allocate(device, &mai, nullptr, &imageMemory);
			if (result != VK_SUCCESS || bindImage(device, image, imageMemory, 0) != VK_SUCCESS) {
				status = "GPU BC7: alocacao/bind da imagem falhou";
					break;
			}
			VkBufferCreateInfo bi{};
				bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bi.size = rgba.size();
				bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			result = createBuffer(device, &bi, nullptr, &staging);
			if (result != VK_SUCCESS) { status = "GPU BC7: staging buffer falhou";
				break;
				}
			VkMemoryRequirements br{};
				getBufferReq(device, staging, &br);
			uint32_t bm = chooseMemory(br.memoryTypeBits,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			if (bm == UINT32_MAX) { status = "GPU BC7: sem memoria HOST_COHERENT";
				break;
				}
			mai.allocationSize = br.size;
				mai.memoryTypeIndex = bm;
			result = allocate(device, &mai, nullptr, &stagingMemory);
			if (result != VK_SUCCESS || bindBuffer(device, staging, stagingMemory, 0) != VK_SUCCESS) {
				status = "GPU BC7: memoria staging falhou";
					break;
			}
			void* mapped = nullptr;
			result = map(device, stagingMemory, 0, rgba.size(), 0, &mapped);
			if (result != VK_SUCCESS || !mapped) { status = "GPU BC7: map staging falhou";
				break;
				}
			std::memcpy(mapped, rgba.data(), rgba.size());
			unmap(device, stagingMemory);
			VkCommandPoolCreateInfo pci{};
				pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			pci.queueFamilyIndex = family;
			result = createPool(device, &pci, nullptr, &pool);
			if (result != VK_SUCCESS) { status = "GPU BC7: command pool falhou";
				break;
				}
			VkCommandBufferAllocateInfo cai{};
				cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			cai.commandPool = pool;
				cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				cai.commandBufferCount = 1;
			result = allocCmd(device, &cai, &cmd);
			if (result != VK_SUCCESS) { status = "GPU BC7: command buffer falhou";
				break;
				}
			VkCommandBufferBeginInfo cbi{};
				cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			result = begin(cmd, &cbi);
			if (result != VK_SUCCESS) { status = "GPU BC7: begin falhou";
				break;
				}
			VkImageMemoryBarrier ib{};
				ib.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			ib.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			ib.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			ib.image = image;
				ib.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			ib.subresourceRange.levelCount = 1;
				ib.subresourceRange.layerCount = 1;
			barrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &ib);
			VkBufferImageCopy region{};
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent = {4,4,1};
			copy(cmd, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			ib.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			ib.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			ib.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			ib.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &ib);
			result = end(cmd);
			if (result != VK_SUCCESS) { status = "GPU BC7: end falhou";
				break;
				}
			VkQueue queue = VK_NULL_HANDLE;
				getQueue(device, family, 0, &queue);
			VkSubmitInfo si{};
				si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			si.commandBufferCount = 1;
				si.pCommandBuffers = &cmd;
			result = submit(queue, 1, &si, VK_NULL_HANDLE);
			if (result != VK_SUCCESS) { status = "GPU BC7: submit falhou";
				break;
				}
			result = wait(queue);
			if (result != VK_SUCCESS) { status = "GPU BC7: queue wait falhou";
				break;
				}
			status = "BC7 GPU: OK\\nBC7 CPU -> RGBA8 -> staging -> imagem Vulkan\\nFormato fisico: VK_FORMAT_R8G8B8A8_UNORM";
			LOG("BC7 -> RGBA8 upload completo na GPU");
		} while (false);
	}

	if (device) {
		auto dp = [&](const char* n) { return gdpa(device, n);
			};
		auto deviceWait = reinterpret_cast<PFN_vkDeviceWaitIdle>(dp("vkDeviceWaitIdle"));
		if (deviceWait) deviceWait(device);
		auto destroyPool = reinterpret_cast<PFN_vkDestroyCommandPool>(dp("vkDestroyCommandPool"));
		auto destroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(dp("vkDestroyBuffer"));
		auto destroyImage = reinterpret_cast<PFN_vkDestroyImage>(dp("vkDestroyImage"));
		auto freeMemory = reinterpret_cast<PFN_vkFreeMemory>(dp("vkFreeMemory"));
		auto destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(dp("vkDestroyDevice"));
		if (pool && destroyPool) destroyPool(device, pool, nullptr);
		if (staging && destroyBuffer) destroyBuffer(device, staging, nullptr);
		if (image && destroyImage) destroyImage(device, image, nullptr);
		if (stagingMemory && freeMemory) freeMemory(device, stagingMemory, nullptr);
		if (imageMemory && freeMemory) freeMemory(device, imageMemory, nullptr);
		if (destroyDevice) destroyDevice(device, nullptr);
	}
	if (destroyInstance) destroyInstance(instance, nullptr);
	dlclose(lib);
	LOG("%s", status.c_str());
	return env->NewStringUTF(status.c_str());
}
