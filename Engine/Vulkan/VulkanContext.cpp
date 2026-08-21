#include "VulkanContext.h"

#include "../Platform/Window.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include <GLFW/glfw3.h>

namespace
{
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"};

const std::vector<const char*> requiredDeviceExtensions = {
	vk::KHRSwapchainExtensionName};
}

VulkanContext::VulkanContext(const Window& window, bool enableValidationLayers)
	: validationEnabled(enableValidationLayers)
{
	createInstance();
	setupDebugMessenger();
	createSurface(window);
	pickPhysicalDevice();
	createLogicalDevice();
}

vk::raii::SurfaceKHR& VulkanContext::surface() noexcept
{
	return surfaceHandle;
}

vk::raii::PhysicalDevice& VulkanContext::physicalDevice() noexcept
{
	return physicalDeviceHandle;
}

vk::raii::Device& VulkanContext::device() noexcept
{
	return deviceHandle;
}

vk::raii::Queue& VulkanContext::queue() noexcept
{
	return queueHandle;
}

uint32_t VulkanContext::queueFamilyIndex() const noexcept
{
	return queueIndex;
}

vk::SampleCountFlagBits VulkanContext::msaaSamples() const noexcept
{
	return sampleCount;
}

void VulkanContext::createInstance()
{
	constexpr vk::ApplicationInfo appInfo{
		.pApplicationName = "Hello Triangle",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = vk::ApiVersion14};

	std::vector<const char*> requiredLayers;
	if (validationEnabled)
	{
		requiredLayers.assign(validationLayers.begin(), validationLayers.end());
	}

	const auto layerProperties = context.enumerateInstanceLayerProperties();
	const auto unsupportedLayer = std::ranges::find_if(
		requiredLayers,
		[&layerProperties](const char* requiredLayer)
		{
			return std::ranges::none_of(
				layerProperties,
				[requiredLayer](const vk::LayerProperties& layer)
				{
					return std::strcmp(layer.layerName, requiredLayer) == 0;
				});
		});
	if (unsupportedLayer != requiredLayers.end())
	{
		throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayer));
	}

	const auto requiredExtensions = getRequiredInstanceExtensions();
	const auto extensionProperties = context.enumerateInstanceExtensionProperties();
	const auto unsupportedExtension = std::ranges::find_if(
		requiredExtensions,
		[&extensionProperties](const char* requiredExtension)
		{
			return std::ranges::none_of(
				extensionProperties,
				[requiredExtension](const vk::ExtensionProperties& extension)
				{
					return std::strcmp(extension.extensionName, requiredExtension) == 0;
				});
		});
	if (unsupportedExtension != requiredExtensions.end())
	{
		throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedExtension));
	}

	const vk::InstanceCreateInfo createInfo{
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
		.ppEnabledLayerNames = requiredLayers.data(),
		.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
		.ppEnabledExtensionNames = requiredExtensions.data()};
	instanceHandle = vk::raii::Instance(context, createInfo);
}

void VulkanContext::setupDebugMessenger()
{
	if (!validationEnabled)
	{
		return;
	}

	const vk::DebugUtilsMessageSeverityFlagsEXT severityFlags =
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
	const vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags =
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
	const vk::DebugUtilsMessengerCreateInfoEXT createInfo{
		.messageSeverity = severityFlags,
		.messageType = messageTypeFlags,
		.pfnUserCallback = &debugCallback};
	debugMessenger = instanceHandle.createDebugUtilsMessengerEXT(createInfo);
}

void VulkanContext::createSurface(const Window& window)
{
	VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
	if (glfwCreateWindowSurface(*instanceHandle, window.nativeHandle(), nullptr, &rawSurface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface!");
	}
	surfaceHandle = vk::raii::SurfaceKHR(instanceHandle, rawSurface);
}

bool VulkanContext::isDeviceSuitable(const vk::raii::PhysicalDevice& candidate) const
{
	const bool supportsVulkan1_3 = candidate.getProperties().apiVersion >= VK_API_VERSION_1_3;

	const auto queueFamilies = candidate.getQueueFamilyProperties();
	bool supportsGraphicsComputePresent = false;
	for (uint32_t index = 0; index < queueFamilies.size(); ++index)
	{
		const vk::QueueFlags flags = queueFamilies[index].queueFlags;
		if ((flags & vk::QueueFlagBits::eGraphics) &&
			(flags & vk::QueueFlagBits::eCompute) &&
			candidate.getSurfaceSupportKHR(index, *surfaceHandle))
		{
			supportsGraphicsComputePresent = true;
			break;
		}
	}

	const auto availableExtensions = candidate.enumerateDeviceExtensionProperties();
	const bool supportsRequiredExtensions = std::ranges::all_of(
		requiredDeviceExtensions,
		[&availableExtensions](const char* requiredExtension)
		{
			return std::ranges::any_of(
				availableExtensions,
				[requiredExtension](const vk::ExtensionProperties& extension)
				{
					return std::strcmp(extension.extensionName, requiredExtension) == 0;
				});
		});

	const auto features = candidate.getFeatures2<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan11Features,
		vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
	const bool supportsRequiredFeatures =
		features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
		features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
		features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
		features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState &&
		features.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
		features.get<vk::PhysicalDeviceFeatures2>().features.sampleRateShading &&
		features.get<vk::PhysicalDeviceFeatures2>().features.largePoints;

	return supportsVulkan1_3 &&
		supportsGraphicsComputePresent &&
		supportsRequiredExtensions &&
		supportsRequiredFeatures;
}

void VulkanContext::pickPhysicalDevice()
{
	auto physicalDevices = instanceHandle.enumeratePhysicalDevices();
	const auto selectedDevice = std::ranges::find_if(
		physicalDevices,
		[this](const vk::raii::PhysicalDevice& candidate)
		{
			return isDeviceSuitable(candidate);
		});
	if (selectedDevice == physicalDevices.end())
	{
		throw std::runtime_error("failed to find a suitable GPU!");
	}

	physicalDeviceHandle = *selectedDevice;
	sampleCount = getMaxUsableSampleCount();
}

void VulkanContext::createLogicalDevice()
{
	const auto queueFamilies = physicalDeviceHandle.getQueueFamilyProperties();
	for (uint32_t index = 0; index < queueFamilies.size(); ++index)
	{
		if ((queueFamilies[index].queueFlags & vk::QueueFlagBits::eGraphics) &&
			(queueFamilies[index].queueFlags & vk::QueueFlagBits::eCompute) &&
			physicalDeviceHandle.getSurfaceSupportKHR(index, *surfaceHandle))
		{
			queueIndex = index;
			break;
		}
	}
	if (queueIndex == ~0u)
	{
		throw std::runtime_error("Could not find a queue for graphics, compute, and present -> terminating");
	}

	vk::StructureChain<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan11Features,
		vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		featureChain = {
			{.features = {.sampleRateShading = true, .largePoints = true, .samplerAnisotropy = true}},
			{.shaderDrawParameters = true},
			{.synchronization2 = true, .dynamicRendering = true},
			{.extendedDynamicState = true}};

	constexpr float queuePriority = 0.5f;
	const vk::DeviceQueueCreateInfo queueCreateInfo{
		.queueFamilyIndex = queueIndex,
		.queueCount = 1,
		.pQueuePriorities = &queuePriority};
	const vk::DeviceCreateInfo deviceCreateInfo{
		.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queueCreateInfo,
		.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
		.ppEnabledExtensionNames = requiredDeviceExtensions.data()};

	deviceHandle = vk::raii::Device(physicalDeviceHandle, deviceCreateInfo);
	queueHandle = vk::raii::Queue(deviceHandle, queueIndex, 0);
}

vk::SampleCountFlagBits VulkanContext::getMaxUsableSampleCount() const
{
	const vk::PhysicalDeviceProperties properties = physicalDeviceHandle.getProperties();
	const vk::SampleCountFlags counts =
		properties.limits.framebufferColorSampleCounts &
		properties.limits.framebufferDepthSampleCounts;

	if (counts & vk::SampleCountFlagBits::e64) return vk::SampleCountFlagBits::e64;
	if (counts & vk::SampleCountFlagBits::e32) return vk::SampleCountFlagBits::e32;
	if (counts & vk::SampleCountFlagBits::e16) return vk::SampleCountFlagBits::e16;
	if (counts & vk::SampleCountFlagBits::e8) return vk::SampleCountFlagBits::e8;
	if (counts & vk::SampleCountFlagBits::e4) return vk::SampleCountFlagBits::e4;
	if (counts & vk::SampleCountFlagBits::e2) return vk::SampleCountFlagBits::e2;
	return vk::SampleCountFlagBits::e1;
}

std::vector<const char*> VulkanContext::getRequiredInstanceExtensions() const
{
	uint32_t extensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extensionCount);
	if (validationEnabled)
	{
		extensions.push_back(vk::EXTDebugUtilsExtensionName);
	}
	return extensions;
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanContext::debugCallback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
	vk::DebugUtilsMessageTypeFlagsEXT type,
	const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
	void*)
{
	if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
		severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
	{
		std::cerr << "validation layer: type " << vk::to_string(type)
			<< " msg: " << callbackData->pMessage << std::endl;
	}
	return vk::False;
}
