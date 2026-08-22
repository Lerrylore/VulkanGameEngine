#include "TextureResource.h"

#include <utility>

TextureResource::TextureResource(
	std::string resourceId,
	VulkanContext& vulkan,
	uint32_t width,
	uint32_t height,
	uint32_t mipLevels,
	vk::Format format)
	: Resource(std::move(resourceId)),
	  imageAllocation{
		std::in_place,
		vulkan,
		width,
		height,
		mipLevels,
		vk::SampleCountFlagBits::e1,
		format,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferSrc |
			vk::ImageUsageFlagBits::eTransferDst |
			vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal},
	  mipLevelCount{mipLevels}
{
	const vk::ImageViewCreateInfo viewInfo{
		.image = *imageAllocation->image(),
		.viewType = vk::ImageViewType::e2D,
		.format = format,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = mipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1}};
	view = vk::raii::ImageView(vulkan.device(), viewInfo);

	const vk::PhysicalDeviceProperties properties = vulkan.physicalDevice().getProperties();
	const vk::SamplerCreateInfo samplerInfo{
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eRepeat,
		.addressModeV = vk::SamplerAddressMode::eRepeat,
		.addressModeW = vk::SamplerAddressMode::eRepeat,
		.mipLodBias = 0.0f,
		.anisotropyEnable = vk::True,
		.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
		.compareEnable = vk::False,
		.compareOp = vk::CompareOp::eAlways,
		.minLod = 0.0f,
		.maxLod = static_cast<float>(mipLevels),
		.borderColor = vk::BorderColor::eIntOpaqueBlack,
		.unnormalizedCoordinates = vk::False};
	samplerHandle = vk::raii::Sampler(vulkan.device(), samplerInfo);
}

vk::raii::Image& TextureResource::image() noexcept
{
	return imageAllocation->image();
}

const vk::raii::Image& TextureResource::image() const noexcept
{
	return imageAllocation->image();
}

const vk::raii::ImageView& TextureResource::imageView() const noexcept
{
	return view;
}

const vk::raii::Sampler& TextureResource::sampler() const noexcept
{
	return samplerHandle;
}

uint32_t TextureResource::mipLevels() const noexcept
{
	return mipLevelCount;
}

bool TextureResource::DoLoad()
{
	return imageAllocation.has_value();
}

void TextureResource::DoUnload()
{
	samplerHandle = nullptr;
	view = nullptr;
	imageAllocation.reset();
}
