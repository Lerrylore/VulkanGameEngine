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

	[[nodiscard]] vk::raii::Instance& instance() noexcept;
	[[nodiscard]] vk::raii::SurfaceKHR& surface() noexcept;

  private:
	void createInstance();
	void setupDebugMessenger();
	void createSurface(const Window& window);

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
};
