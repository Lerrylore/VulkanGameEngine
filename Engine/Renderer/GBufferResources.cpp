#include "GBufferResources.h"

#include <array>
#include <stdexcept>

GBufferResources::GBufferResources(
	VulkanContext& vulkan,
	const SwapchainResources& swapchain,
	uint32_t framesInFlight)
	: Vulkan(vulkan), Swapchain(swapchain), FramesInFlight(framesInFlight), SelectedDepthFormat(FindDepthFormat())
{
	if (FramesInFlight == 0)
	{
		throw std::invalid_argument("GBufferResources requires at least one frame in flight");
	}
	Recreate();
}

void GBufferResources::Recreate()
{
	Frames.clear();
	Frames.reserve(FramesInFlight);
	for (uint32_t frameIndex = 0; frameIndex < FramesInFlight; ++frameIndex)
	{
		auto frame = std::make_unique<Frame>();
		CreateFrame(*frame);
		Frames.push_back(std::move(frame));
	}
}

const vk::raii::Image& GBufferResources::ColorImage(uint32_t frameIndex, std::size_t attachment) const
{
	ValidateFrameIndex(frameIndex);
	ValidateAttachmentIndex(attachment);
	return Frames[frameIndex]->Colors[attachment]->image();
}

const vk::raii::ImageView& GBufferResources::ColorView(uint32_t frameIndex, std::size_t attachment) const
{
	ValidateFrameIndex(frameIndex);
	ValidateAttachmentIndex(attachment);
	return Frames[frameIndex]->ColorViews[attachment];
}

const vk::raii::Image& GBufferResources::DepthImage(uint32_t frameIndex) const
{
	ValidateFrameIndex(frameIndex);
	return Frames[frameIndex]->Depth->image();
}

const vk::raii::ImageView& GBufferResources::DepthView(uint32_t frameIndex) const
{
	ValidateFrameIndex(frameIndex);
	return Frames[frameIndex]->DepthView;
}

vk::Format GBufferResources::ColorFormat(std::size_t attachment) const noexcept
{
	return Formats[attachment];
}

vk::Format GBufferResources::DepthFormat() const noexcept
{
	return SelectedDepthFormat;
}

uint32_t GBufferResources::FrameCount() const noexcept
{
	return FramesInFlight;
}

void GBufferResources::CreateFrame(Frame& frame)
{
	const vk::Extent2D extent = Swapchain.extent();
	frame.ColorViews.reserve(AttachmentCount);
	for (std::size_t attachment = 0; attachment < AttachmentCount; ++attachment)
	{
		frame.Colors[attachment] = std::make_unique<ImageAllocation>(
			Vulkan,
			extent.width,
			extent.height,
			1,
			vk::SampleCountFlagBits::e1,
			Formats[attachment],
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		frame.ColorViews.push_back(CreateView(
			frame.Colors[attachment]->image(),
			Formats[attachment],
			vk::ImageAspectFlagBits::eColor));
	}

	frame.Depth = std::make_unique<ImageAllocation>(
		Vulkan,
		extent.width,
		extent.height,
		1,
		vk::SampleCountFlagBits::e1,
		SelectedDepthFormat,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal);
	frame.DepthView = CreateView(
		frame.Depth->image(),
		SelectedDepthFormat,
		vk::ImageAspectFlagBits::eDepth);
}

vk::raii::ImageView GBufferResources::CreateView(
	const vk::raii::Image& image,
	vk::Format format,
	vk::ImageAspectFlags aspect) const
{
	return vk::raii::ImageView(
		Vulkan.device(),
		vk::ImageViewCreateInfo{
			.image = *image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = {
				.aspectMask = aspect,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1}});
}

vk::Format GBufferResources::FindDepthFormat() const
{
	constexpr std::array candidates{
		vk::Format::eD32Sfloat,
		vk::Format::eD32SfloatS8Uint,
		vk::Format::eD24UnormS8Uint};
	for (const vk::Format format : candidates)
	{
		const vk::FormatProperties properties = Vulkan.physicalDevice().getFormatProperties(format);
		if ((properties.optimalTilingFeatures &
			(vk::FormatFeatureFlagBits::eDepthStencilAttachment | vk::FormatFeatureFlagBits::eSampledImage)) ==
			(vk::FormatFeatureFlagBits::eDepthStencilAttachment | vk::FormatFeatureFlagBits::eSampledImage))
		{
			return format;
		}
	}
	throw std::runtime_error("failed to find a sampled G-buffer depth format");
}

void GBufferResources::ValidateFrameIndex(uint32_t frameIndex) const
{
	if (frameIndex >= Frames.size())
	{
		throw std::out_of_range("GBufferResources frame index is out of range");
	}
}

void GBufferResources::ValidateAttachmentIndex(std::size_t attachment) const
{
	if (attachment >= AttachmentCount)
	{
		throw std::out_of_range("GBufferResources attachment index is out of range");
	}
}
