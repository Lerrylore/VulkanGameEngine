#include "BufferAllocation.h"

#include <stdexcept>

BufferAllocation::BufferAllocation(
	VulkanContext& vulkan,
	vk::DeviceSize size,
	vk::BufferUsageFlags usage,
	vk::MemoryPropertyFlags memoryProperties)
{
	const vk::BufferCreateInfo createInfo{
		.size = size,
		.usage = usage,
		.sharingMode = vk::SharingMode::eExclusive};
	bufferHandle = vk::raii::Buffer(vulkan.device(), createInfo);

	const vk::MemoryRequirements requirements = bufferHandle.getMemoryRequirements();
	const vk::MemoryAllocateInfo allocateInfo{
		.allocationSize = requirements.size,
		.memoryTypeIndex = findMemoryType(
			vulkan.physicalDevice(),
			requirements.memoryTypeBits,
			memoryProperties)};
	memoryHandle = vk::raii::DeviceMemory(vulkan.device(), allocateInfo);
	bufferHandle.bindMemory(*memoryHandle, 0);
}

vk::raii::Buffer& BufferAllocation::buffer() noexcept
{
	return bufferHandle;
}

const vk::raii::Buffer& BufferAllocation::buffer() const noexcept
{
	return bufferHandle;
}

vk::raii::DeviceMemory& BufferAllocation::memory() noexcept
{
	return memoryHandle;
}

const vk::raii::DeviceMemory& BufferAllocation::memory() const noexcept
{
	return memoryHandle;
}

uint32_t BufferAllocation::findMemoryType(
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
	throw std::runtime_error("failed to find suitable memory type for buffer!");
}
