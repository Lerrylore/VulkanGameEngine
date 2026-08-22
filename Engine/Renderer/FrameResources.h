#pragma once

#include "../Vulkan/VulkanContext.h"

#include <cstddef>
#include <cstdint>
#include <vector>

class FrameResources final
{
  public:
	FrameResources(
		VulkanContext& vulkan,
		uint32_t framesInFlight,
		std::size_t swapchainImageCount);

	FrameResources(const FrameResources&) = delete;
	FrameResources& operator=(const FrameResources&) = delete;
	FrameResources(FrameResources&&) = delete;
	FrameResources& operator=(FrameResources&&) = delete;

	[[nodiscard]] vk::raii::CommandPool& commandPool() noexcept;
	[[nodiscard]] vk::raii::CommandBuffer& graphicsCommandBuffer(uint32_t frame);
	[[nodiscard]] vk::raii::CommandBuffer& computeCommandBuffer(uint32_t frame);
	[[nodiscard]] vk::raii::Semaphore& imageAvailableSemaphore(uint32_t frame);
	[[nodiscard]] vk::raii::Semaphore& computeFinishedSemaphore(uint32_t frame);
	[[nodiscard]] vk::raii::Semaphore& renderFinishedSemaphore(uint32_t imageIndex);
	[[nodiscard]] vk::raii::Fence& inFlightFence(uint32_t frame);

	[[nodiscard]] uint32_t currentFrame() const noexcept;
	void advanceFrame() noexcept;
	void recreateSwapchainImages(std::size_t imageCount);

  private:
	vk::raii::Device& device;
	uint32_t frameCount;
	uint32_t currentFrameIndex = 0;

	// Command buffers must be destroyed before the pool that allocated them.
	vk::raii::CommandPool commandPoolHandle = nullptr;
	std::vector<vk::raii::CommandBuffer> graphicsCommandBuffers;
	std::vector<vk::raii::CommandBuffer> computeCommandBuffers;

	std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
	std::vector<vk::raii::Semaphore> computeFinishedSemaphores;
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
	std::vector<vk::raii::Fence> inFlightFences;
};
