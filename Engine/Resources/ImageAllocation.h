#pragma once

#include "../Vulkan/VulkanContext.h"

class ImageAllocation final
{
  public:
	ImageAllocation(
		VulkanContext& vulkan,
		uint32_t width,
		uint32_t height,
		uint32_t mipLevels,
		vk::SampleCountFlagBits samples,
		vk::Format format,
		vk::ImageTiling tiling,
		vk::ImageUsageFlags usage,
		vk::MemoryPropertyFlags memoryProperties);

	ImageAllocation(const ImageAllocation&) = delete;
	ImageAllocation& operator=(const ImageAllocation&) = delete;
	ImageAllocation(ImageAllocation&&) = delete;
	ImageAllocation& operator=(ImageAllocation&&) = delete;

	[[nodiscard]] vk::raii::Image& image() noexcept;
	[[nodiscard]] const vk::raii::Image& image() const noexcept;

  private:
	[[nodiscard]] static uint32_t findMemoryType(
		const vk::raii::PhysicalDevice& physicalDevice,
		uint32_t typeFilter,
		vk::MemoryPropertyFlags properties);

	// Reverse destruction order keeps the bound memory alive until after image.
	vk::raii::DeviceMemory memoryHandle = nullptr;
	vk::raii::Image imageHandle = nullptr;
};
