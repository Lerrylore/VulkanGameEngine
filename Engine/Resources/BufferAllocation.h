#pragma once

#include "../Vulkan/VulkanContext.h"

class BufferAllocation final
{
  public:
	BufferAllocation(
		VulkanContext& vulkan,
		vk::DeviceSize size,
		vk::BufferUsageFlags usage,
		vk::MemoryPropertyFlags memoryProperties);

	BufferAllocation(const BufferAllocation&) = delete;
	BufferAllocation& operator=(const BufferAllocation&) = delete;
	BufferAllocation(BufferAllocation&&) noexcept = default;
	BufferAllocation& operator=(BufferAllocation&&) = delete;

	[[nodiscard]] vk::raii::Buffer& buffer() noexcept;
	[[nodiscard]] const vk::raii::Buffer& buffer() const noexcept;
	[[nodiscard]] vk::raii::DeviceMemory& memory() noexcept;
	[[nodiscard]] const vk::raii::DeviceMemory& memory() const noexcept;

  private:
	[[nodiscard]] static uint32_t findMemoryType(
		const vk::raii::PhysicalDevice& physicalDevice,
		uint32_t typeFilter,
		vk::MemoryPropertyFlags properties);

	// Reverse destruction order keeps the bound memory alive until after buffer.
	vk::raii::DeviceMemory memoryHandle = nullptr;
	vk::raii::Buffer bufferHandle = nullptr;
};
