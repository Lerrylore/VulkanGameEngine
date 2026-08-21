#include "RenderTargetResources.h"

#include <array>
#include <stdexcept>

RenderTargetResources::RenderTargetResources(
	VulkanContext& vulkan,
	const SwapchainResources& swapchain)
	: vulkan(vulkan), swapchain(swapchain), selectedDepthFormat(findDepthFormat())
{
	createColorAttachment();
	createDepthAttachment();
}

void RenderTargetResources::AttachmentImage::reset()
{
	view = nullptr;
	image = nullptr;
	memory = nullptr;
}

void RenderTargetResources::recreate()
{
	colorAttachment.reset();
	depthAttachment.reset();
	createColorAttachment();
	createDepthAttachment();
}

vk::raii::Image& RenderTargetResources::colorImage() noexcept
{
	return colorAttachment.image;
}

vk::raii::ImageView& RenderTargetResources::colorImageView() noexcept
{
	return colorAttachment.view;
}

vk::raii::Image& RenderTargetResources::depthImage() noexcept
{
	return depthAttachment.image;
}

vk::raii::ImageView& RenderTargetResources::depthImageView() noexcept
{
	return depthAttachment.view;
}

vk::Format RenderTargetResources::depthFormat() const noexcept
{
	return selectedDepthFormat;
}

void RenderTargetResources::createColorAttachment()
{
	createAttachmentImage(
		colorAttachment,
		swapchain.surfaceFormat().format,
		vk::ImageUsageFlagBits::eTransientAttachment |
			vk::ImageUsageFlagBits::eColorAttachment);
	colorAttachment.view = createImageView(
		colorAttachment.image,
		swapchain.surfaceFormat().format,
		vk::ImageAspectFlagBits::eColor);
}

void RenderTargetResources::createDepthAttachment()
{
	createAttachmentImage(
		depthAttachment,
		selectedDepthFormat,
		vk::ImageUsageFlagBits::eDepthStencilAttachment);
	depthAttachment.view = createImageView(
		depthAttachment.image,
		selectedDepthFormat,
		vk::ImageAspectFlagBits::eDepth);
}

void RenderTargetResources::createAttachmentImage(
	AttachmentImage& attachment,
	vk::Format format,
	vk::ImageUsageFlags usage)
{
	const vk::Extent2D extent = swapchain.extent();
	const vk::ImageCreateInfo createInfo{
		.imageType = vk::ImageType::e2D,
		.format = format,
		.extent = {extent.width, extent.height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = vulkan.msaaSamples(),
		.tiling = vk::ImageTiling::eOptimal,
		.usage = usage,
		.sharingMode = vk::SharingMode::eExclusive};
	attachment.image = vk::raii::Image(vulkan.device(), createInfo);

	const vk::MemoryRequirements requirements = attachment.image.getMemoryRequirements();
	const vk::MemoryAllocateInfo allocateInfo{
		.allocationSize = requirements.size,
		.memoryTypeIndex = findMemoryType(
			requirements.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eDeviceLocal)};
	attachment.memory = vk::raii::DeviceMemory(vulkan.device(), allocateInfo);
	attachment.image.bindMemory(*attachment.memory, 0);
}

vk::raii::ImageView RenderTargetResources::createImageView(
	const vk::raii::Image& image,
	vk::Format format,
	vk::ImageAspectFlags aspectFlags) const
{
	const vk::ImageViewCreateInfo createInfo{
		.image = *image,
		.viewType = vk::ImageViewType::e2D,
		.format = format,
		.subresourceRange = {
			.aspectMask = aspectFlags,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1}};
	return vk::raii::ImageView(vulkan.device(), createInfo);
}

vk::Format RenderTargetResources::findDepthFormat() const
{
	constexpr std::array candidates{
		vk::Format::eD32Sfloat,
		vk::Format::eD32SfloatS8Uint,
		vk::Format::eD24UnormS8Uint};
	for (const vk::Format format : candidates)
	{
		const vk::FormatProperties properties =
			vulkan.physicalDevice().getFormatProperties(format);
		if ((properties.optimalTilingFeatures &
			 vk::FormatFeatureFlagBits::eDepthStencilAttachment) ==
			vk::FormatFeatureFlagBits::eDepthStencilAttachment)
		{
			return format;
		}
	}
	throw std::runtime_error("failed to find supported depth format!");
}

uint32_t RenderTargetResources::findMemoryType(
	uint32_t typeFilter,
	vk::MemoryPropertyFlags properties) const
{
	const vk::PhysicalDeviceMemoryProperties memoryProperties =
		vulkan.physicalDevice().getMemoryProperties();
	for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index)
	{
		if ((typeFilter & (1u << index)) &&
			(memoryProperties.memoryTypes[index].propertyFlags & properties) == properties)
		{
			return index;
		}
	}
	throw std::runtime_error("failed to find suitable memory type for render target!");
}
