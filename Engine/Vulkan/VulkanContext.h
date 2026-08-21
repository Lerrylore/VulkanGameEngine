#pragma once

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

#ifndef VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#endif

#include <vulkan/vulkan_raii.hpp>

#include <vector>

class Window;

class VulkanContext final
{
  public:
	VulkanContext(const Window& window, bool enableValidationLayers);

	VulkanContext(const VulkanContext&) = delete;
	VulkanContext& operator=(const VulkanContext&) = delete;
	VulkanContext(VulkanContext&&) = delete;
	VulkanContext& operator=(VulkanContext&&) = delete;

	[[nodiscard]] vk::raii::SurfaceKHR& surface() noexcept;
	[[nodiscard]] vk::raii::PhysicalDevice& physicalDevice() noexcept;
	[[nodiscard]] vk::raii::Device& device() noexcept;
	[[nodiscard]] vk::raii::Queue& queue() noexcept;
	[[nodiscard]] uint32_t queueFamilyIndex() const noexcept;
	[[nodiscard]] vk::SampleCountFlagBits msaaSamples() const noexcept;

  private:
	void createInstance();
	void setupDebugMessenger();
	void createSurface(const Window& window);
	[[nodiscard]] bool isDeviceSuitable(const vk::raii::PhysicalDevice& candidate) const;
	void pickPhysicalDevice();
	void createLogicalDevice();
	[[nodiscard]] vk::SampleCountFlagBits getMaxUsableSampleCount() const;

	[[nodiscard]] std::vector<const char*> getRequiredInstanceExtensions() const;
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
		vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
		vk::DebugUtilsMessageTypeFlagsEXT type,
		const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
		void* userData);

	bool validationEnabled = false;
	vk::raii::Context context;
	vk::raii::Instance instanceHandle = nullptr;
	vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
	vk::raii::SurfaceKHR surfaceHandle = nullptr;
	vk::raii::PhysicalDevice physicalDeviceHandle = nullptr;
	vk::raii::Device deviceHandle = nullptr;
	uint32_t queueIndex = ~0u;
	vk::raii::Queue queueHandle = nullptr;
	vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
};
