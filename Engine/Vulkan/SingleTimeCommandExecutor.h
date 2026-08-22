#pragma once

#include "VulkanContext.h"

class SingleTimeCommandExecutor final
{
  public:
	SingleTimeCommandExecutor(
		VulkanContext& vulkan,
		vk::raii::CommandPool& commandPool) noexcept;

	SingleTimeCommandExecutor(const SingleTimeCommandExecutor&) = delete;
	SingleTimeCommandExecutor& operator=(const SingleTimeCommandExecutor&) = delete;
	SingleTimeCommandExecutor(SingleTimeCommandExecutor&&) = delete;
	SingleTimeCommandExecutor& operator=(SingleTimeCommandExecutor&&) = delete;

	[[nodiscard]] vk::raii::CommandBuffer Begin() const;
	void End(vk::raii::CommandBuffer&& commandBuffer) const;
	void CopyBuffer(
		vk::raii::Buffer& source,
		vk::raii::Buffer& destination,
		vk::DeviceSize size) const;

  private:
	VulkanContext& Vulkan;
	vk::raii::CommandPool& CommandPool;
};
