#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>
#include <assert.h>

// Global Settings
const char                      APPNAME[] = "VulkanDemo";
const char                      ENGINENAME[] = "VulkanDemoEngine";
int                             WIDTH = 1280;
int                             HEIGHT = 720;
VkPresentModeKHR                vkPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
VkSurfaceTransformFlagBitsKHR   vkTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
VkFormat                        vkFormat = VK_FORMAT_B8G8R8A8_SRGB;
VkColorSpaceKHR                 vkColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
VkImageUsageFlags               vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

const int MAX_FRAMES_IN_FLIGHT = 2;

#define STRINGIFY( name ) #name

#define Print(...) { printf(__VA_ARGS__); printf("\n"); } 

template<typename T>
T clamp(T value, T min, T max)
{
	return value < min ? min : value > max ? max : value;
}

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

std::string StringifyVulkanVersion(uint32_t version)
{
	if (version > VK_API_VERSION_1_2) return STRINGIFY(VK_VERSION_1_2);
	if (version > VK_API_VERSION_1_1) return STRINGIFY(VK_VERSION_1_1);
	if (version > VK_API_VERSION_1_0) return STRINGIFY(VK_VERSION_1_0);
	return "VK_VERSION_UNKNOWN";
}

// Vulkan Debug Report Callback
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT type,
	uint64_t obj, size_t location, int32_t code, const char* layer,
	const char* message, void* userParam)
{
	if (flags >= VK_DEBUG_REPORT_ERROR_BIT_EXT)
		Print("Vulkan DBG Report: %s - %s", layer, message)
	else if (flags >= VK_DEBUG_REPORT_WARNING_BIT_EXT)
		Print("Vulkan DBG Report: %s - %s", layer, message)
	else
		Print("Vulkan DBG Report: %s - %s", layer, message)
		return VK_FALSE;
}

// Vulkan Debug Utils Messenger
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugUtilsMessager(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		Print("Vulkan DBG Msg: %s", pCallbackData->pMessage)
	else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		Print("Vulkan DBG Msg: %s", pCallbackData->pMessage)
	else
		Print("Vulkan DBG Msg: %s", pCallbackData->pMessage)
		return VK_FALSE;
}

VkResult _vkCreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* createInfo, const VkAllocationCallbacks* allocator, VkDebugReportCallbackEXT* callback)
{
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func)
		return func(instance, createInfo, allocator, callback);
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void _vkDestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* allocator)
{
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func) func(instance, callback, allocator);
}

VkResult _vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func)
		return func(instance, createInfo, allocator, messenger);
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void _vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* allocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func) func(instance, messenger, allocator);
}

void GetAndCheckVulkanAPISupport(uint32_t& version) {
	VkResult result = vkEnumerateInstanceVersion(&version);
	if (result != VK_SUCCESS)
		throw std::exception("Vulkan: API Not Supported");
}

void GetVulkanExtensions(SDL_Window* window, std::vector<const char*>& extensions)
{
	extensions.clear();

	uint32_t extensionCount;
	SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
	extensions.resize(extensionCount);
	SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());

	Print("Vulkan: Found %i Available Extensions", extensionCount);
	for (const auto& extension : extensions) Print("Extension: %s", extension);

#if defined(RAD_DEBUG) || defined(RAD_OPTIMIZED)
	extensions.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
}

void GetVulkanLayerSupport(std::vector<std::string>& enabledLayers)
{
	std::vector<std::string> requestedLayers;
	requestedLayers.push_back("VK_LAYER_KHRONOS_validation");

	enabledLayers.clear();

	if (requestedLayers.empty()) return;

	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	Print("Vulkan: Found %i Available Validation Layers", layerCount);
	for (const auto& availableLayer : availableLayers)
	{
		Print("Vulkan - Layer: %s - %s", availableLayer.layerName, availableLayer.description);
		if (std::find(requestedLayers.begin(), requestedLayers.end(), availableLayer.layerName) != requestedLayers.end())
			enabledLayers.emplace_back(availableLayer.layerName);
	}

	for (const auto& layer : enabledLayers)
		Print("Vulkan - Enabling Layer: %s", layer.c_str());

	if (enabledLayers.size() != requestedLayers.size())
		Print("Vulkan: Could not find all validation layers.");
}

void CreateVulkanInstance(VkInstance& instance, const uint32_t& apiVersion, const std::vector<const char*>& extensions, const std::vector<const char*>& layers)
{
	VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
	appInfo.pApplicationName = APPNAME;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = ENGINENAME;
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = apiVersion;

	VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();
	createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	createInfo.ppEnabledLayerNames = layers.data();

	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS)
	{
		std::string message = "Vulkan: Failed to create instance : ";
		switch (result)
		{
		case VK_ERROR_INCOMPATIBLE_DRIVER: message += STRINGIFY(VK_ERROR_INCOMPATIBLE_DRIVER);
			break;
		case VK_ERROR_LAYER_NOT_PRESENT: message += STRINGIFY(VK_ERROR_LAYER_NOT_PRESENT);
			break;
		case VK_ERROR_EXTENSION_NOT_PRESENT: message += STRINGIFY(VK_ERROR_EXTENSION_NOT_PRESENT);
			break;
		default: message += STRINGIFY(VK_ERROR_UNKNOWN);
			break;
		}
		throw std::exception(message.c_str());
	}

	std::stringstream ss;
	ss << "Loaded Vulkan: " << StringifyVulkanVersion(apiVersion);
	Print(ss.str().c_str());
}

void SetupVulkanDebugMessengerCallback(const VkInstance& instance, VkDebugUtilsMessengerEXT& debugMessenger)
{
	//	Debug Utils Message Callback
	VkDebugUtilsMessengerCreateInfoEXT utilsMessangerInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	utilsMessangerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	utilsMessangerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	utilsMessangerInfo.pfnUserCallback = VulkanDebugUtilsMessager;

	if (_vkCreateDebugUtilsMessengerEXT(instance, &utilsMessangerInfo, nullptr, &debugMessenger) != VK_SUCCESS)
		Print("Vulkan: Unable to create debug utils messenger extension");
}

QueueFamilyIndices GetVulkanQueueFamilies(const VkPhysicalDevice& device, const VkSurfaceKHR& surface)
{
	QueueFamilyIndices indices;

	uint32_t familyQueueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &familyQueueCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyProps(familyQueueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &familyQueueCount, queueFamilyProps.data());

	//	Make sure the family of commands contains an option to issue graphical commands.
	for (uint32_t i = 0; i < familyQueueCount; i++)
	{
		if (queueFamilyProps[i].queueCount > 0 && queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			indices.graphicsFamily = i;

		VkBool32 presentSupport;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (presentSupport)
			indices.presentFamily = i;

		if (indices.isComplete())
			break;
	}

	return indices;
}

bool IsVulkanDeviceCompatible(const VkPhysicalDevice& device, const VkSurfaceKHR& surface)
{
	QueueFamilyIndices indices = GetVulkanQueueFamilies(device, surface);
	return indices.isComplete();
}

void GetVulkanPhysicalDevice(const VkInstance& instance, const VkSurfaceKHR& surface, VkPhysicalDevice& outDevice)
{
	uint32_t physicalDeviceCount;
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

	if (physicalDeviceCount == 0)
		throw std::exception("Vulkan: No Physical Device (GPU) Found.");

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

	Print("Vulkan: Found %i Physical Devices", physicalDeviceCount);
	for (const auto& physicalDevice : physicalDevices)
	{
		VkPhysicalDeviceProperties deviceProps;
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);
		Print("Vulkan - Physical Device: %s", deviceProps.deviceName);
	}

	for (const auto& physicalDevice : physicalDevices)
	{
		if (IsVulkanDeviceCompatible(physicalDevice, surface))
		{
			outDevice = physicalDevice; break;
		}
	}

	if (outDevice == VK_NULL_HANDLE)
		throw std::exception("Vulkan: Unable to fin a compatible GPU");
}

void CreateVulkanLogicalDevice(VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, const std::vector<const char*>& layers, VkDevice& outLogicalDevice, VkQueue& outGraphicsQueue, VkQueue& outPresentQueue)
{
	QueueFamilyIndices indices = GetVulkanQueueFamilies(physicalDevice, surface);

	// Create queue information structure used by device based on the previously fetched queue information from the physical device
	// We create one command processing queue for graphics
	std::vector<float> queuePriority = { 1.0f };

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	uint32_t devicePropertiesCount;
	if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &devicePropertiesCount, nullptr) != VK_SUCCESS)
		throw std::exception("Vulkan: Unable to acquire device extension property count");

	Print("Vulkan: Found %i Device extension properties", devicePropertiesCount);

	std::vector<VkExtensionProperties> deviceProperties(devicePropertiesCount);
	if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &devicePropertiesCount, deviceProperties.data()) != VK_SUCCESS)
		throw std::exception("Vulkan: Unable to acquire device extension properties");

	std::vector<const char*> devicePropertiesNames;
	std::set<std::string> requestedExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	for (const auto& extensionProperty : deviceProperties)
	{
		auto it = requestedExtensions.find(std::string(extensionProperty.extensionName));
		if (it != requestedExtensions.end())
			devicePropertiesNames.emplace_back(extensionProperty.extensionName);
	}

	if (requestedExtensions.size() != devicePropertiesNames.size())
		Print("Vulkan: Could not find all validation layers.");

	for (const auto& devicePropertyName : devicePropertiesNames)
		Print("Vulkan - Enabling Device Extension Property: %s", devicePropertyName);

	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = queuePriority.data();
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Device creation information
	VkDeviceCreateInfo deviceCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.ppEnabledLayerNames = layers.data();
	deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	deviceCreateInfo.ppEnabledExtensionNames = devicePropertiesNames.data();
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(devicePropertiesNames.size());

	if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &outLogicalDevice) != VK_SUCCESS)
		throw std::exception("Vulkan: Failed To create logical device");

	vkGetDeviceQueue(outLogicalDevice, indices.graphicsFamily.value(), 0, &outGraphicsQueue);
	vkGetDeviceQueue(outLogicalDevice, indices.presentFamily.value(), 0, &outPresentQueue);
}

void CreateVulkanSurface(SDL_Window* window, VkInstance instance, VkSurfaceKHR& outSurface)
{
	if (!SDL_Vulkan_CreateSurface(window, instance, &outSurface))
	{
		std::string temp("Vulkan: Failed To create surface. \nReason: ");
		temp.append(SDL_GetError());
		throw std::exception(temp.c_str());
	}
}

void GetVulkanPresentationMode(const VkSurfaceKHR& surface, const VkPhysicalDevice& physicalDevice, VkPresentModeKHR& outMode)
{
	uint32_t modeCount;
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr) != VK_SUCCESS)
		throw std::exception("Vulkan: Unable to query present mode count from physical device.");

	std::vector<VkPresentModeKHR> availableModes(modeCount);
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, availableModes.data()) != VK_SUCCESS)
		throw std::exception("Vulkan: Unable to query present modes from physical device.");

	for (const auto& mode : availableModes)
		if (mode == outMode)
			return;

	Print("Vulkan: unable to use perfered display mode, Falling back to FIFO");
	outMode = VK_PRESENT_MODE_FIFO_KHR;
}

void GetVulkanImageFormat(const VkPhysicalDevice& device, const VkSurfaceKHR& surface, VkSurfaceFormatKHR& outFormat)
{
	uint32_t count;
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr) != VK_SUCCESS)
		throw std::exception("Vulkan: Failed to get Physical Device Surface Formats Count");
	std::vector<VkSurfaceFormatKHR> foundFormats(count);
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, foundFormats.data()) != VK_SUCCESS)
		throw std::exception("Vulkan: Failed to get Physical Devices Surface Formats");

	if (foundFormats.size() == 1 && foundFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		outFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
		outFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return;
	}

	for (const auto& outerFormat : foundFormats)
	{
		if (outerFormat.format == VK_FORMAT_B8G8R8A8_SRGB)
		{
			outFormat.format = outerFormat.format;
			for (const auto& innerFormat : foundFormats)
			{
				if (innerFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				{
					outFormat.colorSpace = innerFormat.colorSpace;
					return;
				}
			}

			Print("Vulkan: No matching color space found, picking first available one!");
			outFormat.colorSpace = foundFormats[0].colorSpace;
			return;
		}
	}

	Print("Vulkan: no matching color format found, picking first available one!");
	outFormat = foundFormats[0];
}

void CreateVulkanSwapchain(const VkSurfaceKHR& surface, const VkPhysicalDevice& physicalDevice, const VkDevice& device, VkSwapchainKHR& outSwapchain, VkSurfaceFormatKHR& outSurfaceFormat, VkExtent2D& outExtent)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities) != VK_SUCCESS)
		throw std::exception("Vulkan: Unable to acquire surface capabilities");

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	GetVulkanPresentationMode(surface, physicalDevice, presentMode);

	uint32_t swapImageCount = surfaceCapabilities.minImageCount + 1 > surfaceCapabilities.maxImageCount ? surfaceCapabilities.minImageCount : surfaceCapabilities.minImageCount + 1;

	VkExtent2D size = { WIDTH, HEIGHT };
	if (surfaceCapabilities.currentExtent.width == 0xFFFFFFF)
	{
		size.width = clamp<uint32_t>(size.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		size.height = clamp<uint32_t>(size.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
	}
	else
		size = surfaceCapabilities.currentExtent;

	VkImageUsageFlags usageFlags;
	std::vector<VkImageUsageFlags> requestedUsages{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT };
	assert(requestedUsages.size() > 0);

	usageFlags = requestedUsages[0];

	for (const auto& requestedUsage : requestedUsages)
	{
		VkImageUsageFlags imageUsage = requestedUsage & surfaceCapabilities.supportedUsageFlags;
		if (imageUsage != requestedUsage)
			throw std::exception("Vulkan: unsupported image usage flag:" + requestedUsage);

		usageFlags |= requestedUsage;
	}

	VkSurfaceTransformFlagBitsKHR transform;
	if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	else
	{
		Print("Vulkan: unsupported surface transform: %s", STRINGIFY(VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR));
		transform = surfaceCapabilities.currentTransform;
	}

	VkSurfaceFormatKHR imageFormat;
	GetVulkanImageFormat(physicalDevice, surface, imageFormat);

	VkSwapchainKHR oldSwapchain = outSwapchain;

	VkSwapchainCreateInfoKHR swapInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapInfo.surface = surface;
	swapInfo.minImageCount = swapImageCount;
	swapInfo.imageFormat = imageFormat.format;
	swapInfo.imageColorSpace = imageFormat.colorSpace;
	swapInfo.imageExtent = size;
	swapInfo.imageArrayLayers = 1;
	swapInfo.imageUsage = usageFlags;
	swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapInfo.queueFamilyIndexCount = 0;
	swapInfo.pQueueFamilyIndices = nullptr;
	swapInfo.preTransform = transform;
	swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapInfo.presentMode = presentMode;
	swapInfo.clipped = true;
	swapInfo.oldSwapchain = nullptr;

	if (oldSwapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
		oldSwapchain = VK_NULL_HANDLE;
	}

	if (vkCreateSwapchainKHR(device, &swapInfo, nullptr, &oldSwapchain) != VK_SUCCESS)
		throw std::exception("Vulkan: Failed to create Swapchain");

	outSwapchain = oldSwapchain;
	outSurfaceFormat = imageFormat;
	outExtent = size;
}

void GetVulkanSwapchainImageHandles(const VkDevice& device, const VkSwapchainKHR& chain, std::vector<VkImage>& outImageHandles)
{
	uint32_t imageCount;
	if (vkGetSwapchainImagesKHR(device, chain, &imageCount, nullptr) != VK_SUCCESS)
		throw std::runtime_error("Vulkan: Failed to get Swapchain Images Count");
	outImageHandles.clear();
	outImageHandles.resize(imageCount);
	if (vkGetSwapchainImagesKHR(device, chain, &imageCount, outImageHandles.data()) != VK_SUCCESS)
		throw std::runtime_error("Vulkan: Failed to get Swapchain Images");
}

void CreateVulkanImageViews(const VkDevice& device, const VkSurfaceFormatKHR& swapchainFormat, const std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& outSwapchainImageViews)
{
	outSwapchainImageViews.clear();
	outSwapchainImageViews.resize(swapchainImages.size());

	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		VkImageViewCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		createInfo.image = swapchainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapchainFormat.format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &createInfo, nullptr, &outSwapchainImageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image views!");
		}
	}
}

VkShaderModule CreateVulkanShaderModule(const VkDevice& device, const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		throw std::runtime_error("failed to create shader module!");

	return shaderModule;
}

std::vector<char> ReadFile(const std::string& path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);

	if (!file.is_open())
		throw std::runtime_error("Failed to Open File!");

	auto fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(),fileSize);
	file.close();
	return buffer;
}

void CreateVulkanGraphicsPipeline(const VkDevice& device, const VkExtent2D& swapchainExtent, const VkRenderPass& renderPass, VkPipelineLayout& outPipelineLayout, VkPipeline& outGraphicsPipeline)
{
	auto vertShaderCode = ReadFile("Shaders/SPIR-V/vert.spv");
	auto fragShaderCode = ReadFile("Shaders/SPIR-V/frag.spv");

	auto vertShaderModule = CreateVulkanShaderModule(device, vertShaderCode);
	auto fragShaderModule = CreateVulkanShaderModule(device, fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapchainExtent.width;
	viewport.height = (float)swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchainExtent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &outPipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("failed to create pipeline layout!");

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = outPipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &outGraphicsPipeline) != VK_SUCCESS)
		throw std::runtime_error("failed to create graphics pipeline!");

	vkDestroyShaderModule(device, fragShaderModule, nullptr);
	vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void CreateVulkanRenderPass(const VkDevice& device, const VkSurfaceFormatKHR& swapchainFormat, VkRenderPass& outRenderPass) {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchainFormat.format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &outRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

void CreateVulkanFramebuffers(const VkDevice& device, const VkExtent2D& extents, const VkRenderPass& renderPass, const std::vector<VkImageView>& swapchainImageViews, std::vector<VkFramebuffer>& outSwapchainFramebuffers) {
	outSwapchainFramebuffers.clear();
	outSwapchainFramebuffers.resize(swapchainImageViews.size());

	for (size_t i = 0; i < swapchainImageViews.size(); i++) {
		VkImageView attachments[] = {
			swapchainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = extents.width;
		framebufferInfo.height = extents.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &outSwapchainFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

void CreateVulkanCommandPool(const VkPhysicalDevice& physicalDevice, const VkDevice& device, const VkSurfaceKHR& surface, VkCommandPool& outCmdPool) {
	QueueFamilyIndices queueFamilyIndices = GetVulkanQueueFamilies(physicalDevice, surface);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &outCmdPool) != VK_SUCCESS)
		throw std::runtime_error("failed to create command pool!");
}

void CreateVulkanCommandBuffers(const VkDevice& device, const VkCommandPool& cmdPool, const VkRenderPass& renderPass,  const VkPipeline& gfxPipeline, const VkExtent2D extents, const std::vector<VkFramebuffer>& framebuffers, std::vector<VkCommandBuffer>& outCmdBuffers) {
	outCmdBuffers.clear();
	outCmdBuffers.resize(framebuffers.size());

	VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocInfo.commandPool = cmdPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)outCmdBuffers.size();

	if (vkAllocateCommandBuffers(device, &allocInfo, outCmdBuffers.data()) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate command buffers!");

	for (size_t i = 0; i < outCmdBuffers.size(); i++) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(outCmdBuffers[i], &beginInfo) != VK_SUCCESS)
			throw std::runtime_error("failed to begin recording command buffer!");

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = framebuffers[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = extents;

		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(outCmdBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(outCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);

		vkCmdDraw(outCmdBuffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(outCmdBuffers[i]);

		if (vkEndCommandBuffer(outCmdBuffers[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to record command buffer!");
	}
}

void CreateVulkanSyncObjects(const VkDevice& device, const std::vector<VkImage>& swapChainImages, std::vector<VkSemaphore>& outImageReadySemaphores, std::vector<VkSemaphore>& outRenderFinishedSemaphores, std::vector<VkFence>& outFlightFences, std::vector<VkFence>& outImagesInFlight) 
{
	outImageReadySemaphores.clear();
	outRenderFinishedSemaphores.clear();
	outFlightFences.clear();
	outImagesInFlight.clear();

	outImageReadySemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	outRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	outFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	outImagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &outImageReadySemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &outRenderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device, &fenceInfo, nullptr, &outFlightFences[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create synchronization objects for a frame!");
	}
}

int main(int argc, char* args[])
{
	//	Local Variables
	uint32_t apiVersion;
	bool isRunning = true;
	std::vector<const char*> extensions{};
	std::vector<std::string> enabledLayers{};
	std::vector<const char*> layers;
	VkInstance vkInstance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT vkDebugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
	VkDevice vkDevice = VK_NULL_HANDLE;
	VkQueue vkGraphicsQueue, vkPresentQueue;
	VkSwapchainKHR vkSwapchain = VK_NULL_HANDLE;
	VkSurfaceFormatKHR vkSurfaceFormat;
	VkExtent2D vkExtent;
	VkRenderPass vkRenderPass = VK_NULL_HANDLE;
	VkPipelineLayout vkPipelineLayout = VK_NULL_HANDLE;
	VkPipeline vkPipeline = VK_NULL_HANDLE;
	VkCommandPool vkCommandPool = VK_NULL_HANDLE;
	std::vector<VkImage> vkChainImages;
	std::vector<VkImageView> vkChainImageViews;
	std::vector<VkFramebuffer> vkChainFramebuffers;
	std::vector<VkCommandBuffer> vkCommandBuffers;
	std::vector<VkSemaphore> vkImageAvailableSemaphores;
	std::vector<VkSemaphore> vkRenderFinishedSemaphores;
	std::vector<VkFence> vkInFlightFences;
	std::vector<VkFence> vkImagesInFlight;
	size_t currentFrame = 0;


	//	=============================================================

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	auto* sdlWindow = SDL_CreateWindow("Hello Vulkan", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);

	try {
		GetAndCheckVulkanAPISupport(apiVersion);

		GetVulkanExtensions(sdlWindow, extensions);

		GetVulkanLayerSupport(enabledLayers);

		for (const auto& layer : enabledLayers)
			layers.emplace_back(layer.c_str());

		CreateVulkanInstance(vkInstance, apiVersion, extensions, layers);

		SetupVulkanDebugMessengerCallback(vkInstance, vkDebugMessenger);

		CreateVulkanSurface(sdlWindow, vkInstance, surface);

		GetVulkanPhysicalDevice(vkInstance, surface, vkPhysicalDevice);

		CreateVulkanLogicalDevice(vkPhysicalDevice, surface, layers, vkDevice, vkGraphicsQueue, vkPresentQueue);

		CreateVulkanSwapchain(surface, vkPhysicalDevice, vkDevice, vkSwapchain, vkSurfaceFormat, vkExtent);

		GetVulkanSwapchainImageHandles(vkDevice, vkSwapchain, vkChainImages);

		CreateVulkanImageViews(vkDevice, vkSurfaceFormat, vkChainImages, vkChainImageViews);

		CreateVulkanRenderPass(vkDevice, vkSurfaceFormat, vkRenderPass);

		CreateVulkanGraphicsPipeline(vkDevice, vkExtent, vkRenderPass, vkPipelineLayout, vkPipeline);

		CreateVulkanFramebuffers(vkDevice, vkExtent, vkRenderPass, vkChainImageViews, vkChainFramebuffers);

		CreateVulkanCommandPool(vkPhysicalDevice, vkDevice, surface, vkCommandPool);

		CreateVulkanCommandBuffers(vkDevice, vkCommandPool, vkRenderPass, vkPipeline, vkExtent, vkChainFramebuffers, vkCommandBuffers);

		CreateVulkanSyncObjects(vkDevice, vkChainImages, vkImageAvailableSemaphores, vkRenderFinishedSemaphores, vkInFlightFences, vkImagesInFlight);

		while (isRunning)
		{
			//	Handle Events
			SDL_Event e;
			SDL_PollEvent(&e);
			switch (e.type)
			{
			case SDL_QUIT: isRunning = false;
				break;
			default: break;
			}

			//	Drawing Code
			vkWaitForFences(vkDevice, 1, &vkInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

			uint32_t imageIndex;
			vkAcquireNextImageKHR(vkDevice, vkSwapchain, UINT64_MAX, vkImageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

			if (vkImagesInFlight[imageIndex] != VK_NULL_HANDLE) {
				vkWaitForFences(vkDevice, 1, &vkImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
			}
			vkImagesInFlight[imageIndex] = vkInFlightFences[currentFrame];

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			VkSemaphore waitSemaphores[] = { vkImageAvailableSemaphores[currentFrame] };
			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;

			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &vkCommandBuffers[imageIndex];

			VkSemaphore signalSemaphores[] = { vkRenderFinishedSemaphores[currentFrame] };
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;

			vkResetFences(vkDevice, 1, &vkInFlightFences[currentFrame]);

			if (vkQueueSubmit(vkGraphicsQueue, 1, &submitInfo, vkInFlightFences[currentFrame]) != VK_SUCCESS) {
				throw std::runtime_error("failed to submit draw command buffer!");
			}

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;

			VkSwapchainKHR swapChains[] = { vkSwapchain };
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChains;-sd

			presentInfo.pImageIndices = &imageIndex;

			vkQueuePresentKHR(vkPresentQueue, &presentInfo);

			currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		}
	}
	catch (std::exception e)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Exception Thrown", e.what(), nullptr);
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(vkDevice, vkRenderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(vkDevice, vkImageAvailableSemaphores[i], nullptr);
		vkDestroyFence(vkDevice, vkInFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(vkDevice, vkCommandPool, nullptr);

	for (auto framebuffer : vkChainFramebuffers)
		vkDestroyFramebuffer(vkDevice, framebuffer, nullptr);

	vkDestroyPipeline(vkDevice, vkPipeline, nullptr);
	vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, nullptr);
	vkDestroyRenderPass(vkDevice, vkRenderPass, nullptr);

	for (auto imageView : vkChainImageViews)
		vkDestroyImageView(vkDevice, imageView, nullptr);

	vkDestroySwapchainKHR(vkDevice, vkSwapchain, nullptr);
	vkDestroyDevice(vkDevice, nullptr);
	vkDestroySurfaceKHR(vkInstance, surface, nullptr);
	_vkDestroyDebugUtilsMessengerEXT(vkInstance, vkDebugMessenger, nullptr);
	vkDestroyInstance(vkInstance, nullptr);

	return 0;
}