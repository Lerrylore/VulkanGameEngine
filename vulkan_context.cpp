#include "vulkan_context.hpp"

#include "window.hpp"

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
}

VulkanContext::VulkanContext(const Window& window, bool enableValidationLayers)
	: validationEnabled(enableValidationLayers)
{
	createInstance();
	setupDebugMessenger();
	createSurface(window);
}

vk::raii::Instance& VulkanContext::instance() noexcept
{
	return instanceHandle;
}

vk::raii::SurfaceKHR& VulkanContext::surface() noexcept
{
	return surfaceHandle;
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
