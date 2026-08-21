#pragma once

#include "../Vulkan/VulkanContext.h"

#include <vector>

class Window;

class SwapchainResources final
{
  public:
	SwapchainResources(VulkanContext& vulkan, const Window& window);

	SwapchainResources(const SwapchainResources&) = delete;
	SwapchainResources& operator=(const SwapchainResources&) = delete;
	SwapchainResources(SwapchainResources&&) = delete;
	SwapchainResources& operator=(SwapchainResources&&) = delete;

	[[nodiscard]] bool recreate();

	[[nodiscard]] vk::raii::SwapchainKHR& handle() noexcept;
	[[nodiscard]] const std::vector<vk::Image>& images() const noexcept;
	[[nodiscard]] const std::vector<vk::raii::ImageView>& imageViews() const noexcept;
	[[nodiscard]] const vk::SurfaceFormatKHR& surfaceFormat() const noexcept;
	[[nodiscard]] const vk::Extent2D& extent() const noexcept;

  private:
	void createSwapchain();
	void createImageViews();

	[[nodiscard]] static uint32_t chooseMinImageCount(const vk::SurfaceCapabilitiesKHR& capabilities);
	[[nodiscard]] static vk::SurfaceFormatKHR chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats);
	[[nodiscard]] static vk::PresentModeKHR choosePresentMode(const std::vector<vk::PresentModeKHR>& presentModes);
	[[nodiscard]] vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;

	VulkanContext& vulkan;
	const Window& window;
	vk::raii::SwapchainKHR swapchain = nullptr;
	std::vector<vk::Image> swapchainImages;
	vk::SurfaceFormatKHR selectedSurfaceFormat;
	vk::Extent2D selectedExtent;
	std::vector<vk::raii::ImageView> swapchainImageViews;
};
