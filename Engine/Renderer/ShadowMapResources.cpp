#include "ShadowMapResources.h"

#include <array>
#include <stdexcept>

ShadowMapResources::ShadowMapResources(
	VulkanContext& vulkan,
	uint32_t framesInFlight,
	uint32_t resolution)
	: FrameCount(framesInFlight), Resolution(resolution), Format(FindDepthFormat(vulkan.physicalDevice()))
{
	if (FrameCount == 0)
	{
		throw std::invalid_argument("ShadowMapResources requires at least one frame in flight");
	}
	if (Resolution == 0)
	{
		throw std::invalid_argument("Shadow map resolution must be positive");
	}

	Images.reserve(FrameCount);
	ImageViews.reserve(FrameCount);
	Samplers.reserve(FrameCount);
	for (uint32_t frameIndex = 0; frameIndex < FrameCount; ++frameIndex)
	{
		Images.push_back(std::make_unique<ImageAllocation>(
			vulkan,
			Resolution,
			Resolution,
			1,
			vk::SampleCountFlagBits::e1,
			Format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eDepthStencilAttachment |
				vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal));

		const vk::ImageViewCreateInfo viewInfo{
			.image = *Images.back()->image(),
			.viewType = vk::ImageViewType::e2D,
			.format = Format,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eDepth,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1}};
		ImageViews.emplace_back(vulkan.device(), viewInfo);

		const vk::SamplerCreateInfo samplerInfo{
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eNearest,
			.addressModeU = vk::SamplerAddressMode::eClampToBorder,
			.addressModeV = vk::SamplerAddressMode::eClampToBorder,
			.addressModeW = vk::SamplerAddressMode::eClampToBorder,
			.compareEnable = vk::False,
			.compareOp = vk::CompareOp::eAlways,
			.borderColor = vk::BorderColor::eFloatOpaqueWhite,
			.unnormalizedCoordinates = vk::False};
		Samplers.emplace_back(vulkan.device(), samplerInfo);
	}
}

const vk::raii::Image& ShadowMapResources::GetImage(uint32_t frameIndex) const noexcept
{
	return Images[frameIndex]->image();
}

const vk::raii::ImageView& ShadowMapResources::GetImageView(uint32_t frameIndex) const noexcept
{
	return ImageViews[frameIndex];
}

const vk::raii::Sampler& ShadowMapResources::GetSampler(uint32_t frameIndex) const noexcept
{
	return Samplers[frameIndex];
}

vk::Format ShadowMapResources::GetFormat() const noexcept
{
	return Format;
}

uint32_t ShadowMapResources::GetResolution() const noexcept
{
	return Resolution;
}

vk::Format ShadowMapResources::FindDepthFormat(
	const vk::raii::PhysicalDevice& physicalDevice)
{
	constexpr std::array candidates{
		vk::Format::eD32Sfloat,
		vk::Format::eD32SfloatS8Uint,
		vk::Format::eD24UnormS8Uint};
	for (const vk::Format format : candidates)
	{
		const vk::FormatProperties properties = physicalDevice.getFormatProperties(format);
		if ((properties.optimalTilingFeatures &
				(vk::FormatFeatureFlagBits::eDepthStencilAttachment |
					vk::FormatFeatureFlagBits::eSampledImage)) ==
			(vk::FormatFeatureFlagBits::eDepthStencilAttachment |
				vk::FormatFeatureFlagBits::eSampledImage))
		{
			return format;
		}
	}
	throw std::runtime_error("failed to find a sampled depth format for shadow maps");
}

void ShadowMapResources::ValidateFrameIndex(uint32_t frameIndex) const
{
	if (frameIndex >= FrameCount)
	{
		throw std::out_of_range("Shadow map frame index is out of range");
	}
}
