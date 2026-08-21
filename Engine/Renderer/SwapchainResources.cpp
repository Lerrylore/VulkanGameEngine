#include "SwapchainResources.h"

#include "../Platform/Window.h"

#include <algorithm>
#include <cassert>
#include <limits>

SwapchainResources::SwapchainResources(VulkanContext& vulkan, const Window& window)
	: vulkan(vulkan), window(window)
{
	createSwapchain();
	createImageViews();
}

bool SwapchainResources::recreate()
{
	const vk::Format previousFormat = selectedSurfaceFormat.format;
	swapchainImageViews.clear();
	swapchainImages.clear();
	swapchain = nullptr;
	createSwapchain();
	createImageViews();
	return selectedSurfaceFormat.format != previousFormat;
}

vk::raii::SwapchainKHR& SwapchainResources::handle() noexcept
{
	return swapchain;
}

const std::vector<vk::Image>& SwapchainResources::images() const noexcept
{
	return swapchainImages;
}

const std::vector<vk::raii::ImageView>& SwapchainResources::imageViews() const noexcept
{
	return swapchainImageViews;
}

const vk::SurfaceFormatKHR& SwapchainResources::surfaceFormat() const noexcept
{
	return selectedSurfaceFormat;
}

const vk::Extent2D& SwapchainResources::extent() const noexcept
{
	return selectedExtent;
}

void SwapchainResources::createSwapchain()
{
	auto& physicalDevice = vulkan.physicalDevice();
	const vk::SurfaceCapabilitiesKHR capabilities =
		physicalDevice.getSurfaceCapabilitiesKHR(*vulkan.surface());
	selectedExtent = chooseExtent(capabilities);
	selectedSurfaceFormat = chooseSurfaceFormat(
		physicalDevice.getSurfaceFormatsKHR(*vulkan.surface()));

	const vk::SwapchainCreateInfoKHR createInfo{
		.surface = *vulkan.surface(),
		.minImageCount = chooseMinImageCount(capabilities),
		.imageFormat = selectedSurfaceFormat.format,
		.imageColorSpace = selectedSurfaceFormat.colorSpace,
		.imageExtent = selectedExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = choosePresentMode(
			physicalDevice.getSurfacePresentModesKHR(*vulkan.surface())),
		.clipped = true};

	swapchain = vk::raii::SwapchainKHR(vulkan.device(), createInfo);
	swapchainImages = swapchain.getImages();
}

void SwapchainResources::createImageViews()
{
	assert(swapchainImageViews.empty());
	swapchainImageViews.reserve(swapchainImages.size());
	for (const vk::Image image : swapchainImages)
	{
		const vk::ImageViewCreateInfo createInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = selectedSurfaceFormat.format,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1}};
		swapchainImageViews.emplace_back(vulkan.device(), createInfo);
	}
}

uint32_t SwapchainResources::chooseMinImageCount(const vk::SurfaceCapabilitiesKHR& capabilities)
{
	uint32_t imageCount = std::max(3u, capabilities.minImageCount);
	if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < imageCount)
	{
		imageCount = capabilities.maxImageCount;
	}
	return imageCount;
}

vk::SurfaceFormatKHR SwapchainResources::chooseSurfaceFormat(
	const std::vector<vk::SurfaceFormatKHR>& formats)
{
	assert(!formats.empty());
	const auto preferredFormat = std::ranges::find_if(
		formats,
		[](const vk::SurfaceFormatKHR& format)
		{
			return format.format == vk::Format::eB8G8R8A8Srgb &&
				format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
		});
	return preferredFormat != formats.end() ? *preferredFormat : formats.front();
}

vk::PresentModeKHR SwapchainResources::choosePresentMode(
	const std::vector<vk::PresentModeKHR>& presentModes)
{
	assert(std::ranges::any_of(
		presentModes,
		[](vk::PresentModeKHR mode) { return mode == vk::PresentModeKHR::eFifo; }));
	const bool supportsMailbox = std::ranges::any_of(
		presentModes,
		[](vk::PresentModeKHR mode) { return mode == vk::PresentModeKHR::eMailbox; });
	return supportsMailbox ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
}

vk::Extent2D SwapchainResources::chooseExtent(
	const vk::SurfaceCapabilitiesKHR& capabilities) const
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}

	const auto [width, height] = window.framebufferSize();
	return {
		std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}
