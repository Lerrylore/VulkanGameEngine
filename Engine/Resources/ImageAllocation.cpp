#include "ImageAllocation.h"

#include <stdexcept>

ImageAllocation::ImageAllocation(
	VulkanContext& vulkan,
	uint32_t width,
	uint32_t height,
	uint32_t mipLevels,
	vk::SampleCountFlagBits samples,
	vk::Format format,
	vk::ImageTiling tiling,
	vk::ImageUsageFlags usage,
	vk::MemoryPropertyFlags memoryProperties)
{
	const vk::ImageCreateInfo createInfo{
		.imageType = vk::ImageType::e2D,
		.format = format,
		.extent = {width, height, 1},
		.mipLevels = mipLevels,
		.arrayLayers = 1,
		.samples = samples,
		.tiling = tiling,
		.usage = usage,
		.sharingMode = vk::SharingMode::eExclusive};
	imageHandle = vk::raii::Image(vulkan.device(), createInfo);

	const vk::MemoryRequirements requirements = imageHandle.getMemoryRequirements();
	const vk::MemoryAllocateInfo allocateInfo{
		.allocationSize = requirements.size,
		.memoryTypeIndex = findMemoryType(
			vulkan.physicalDevice(),
			requirements.memoryTypeBits,
			memoryProperties)};
	memoryHandle = vk::raii::DeviceMemory(vulkan.device(), allocateInfo);
	imageHandle.bindMemory(*memoryHandle, 0);
}

vk::raii::Image& ImageAllocation::image() noexcept
{
	return imageHandle;
}

const vk::raii::Image& ImageAllocation::image() const noexcept
{
	return imageHandle;
}

uint32_t ImageAllocation::findMemoryType(
	const vk::raii::PhysicalDevice& physicalDevice,
	uint32_t typeFilter,
	vk::MemoryPropertyFlags properties)
{
	const vk::PhysicalDeviceMemoryProperties memoryProperties =
		physicalDevice.getMemoryProperties();
	for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index)
	{
		if ((typeFilter & (1u << index)) &&
			(memoryProperties.memoryTypes[index].propertyFlags & properties) == properties)
		{
			return index;
		}
	}
	throw std::runtime_error("failed to find suitable memory type for image!");
}
