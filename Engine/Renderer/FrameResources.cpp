#include "FrameResources.h"

#include <stdexcept>

FrameResources::FrameResources(
	VulkanContext& vulkan,
	uint32_t framesInFlight,
	std::size_t swapchainImageCount)
	: device(vulkan.device()), frameCount(framesInFlight)
{
	if (frameCount == 0)
	{
		throw std::invalid_argument("frames in flight must be greater than zero");
	}

	const vk::CommandPoolCreateInfo poolInfo{
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = vulkan.queueFamilyIndex()};
	commandPoolHandle = vk::raii::CommandPool(device, poolInfo);

	const vk::CommandBufferAllocateInfo allocateInfo{
		.commandPool = commandPoolHandle,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = frameCount};
	graphicsCommandBuffers = vk::raii::CommandBuffers(device, allocateInfo);
	computeCommandBuffers = vk::raii::CommandBuffers(device, allocateInfo);

	imageAvailableSemaphores.reserve(frameCount);
	computeFinishedSemaphores.reserve(frameCount);
	inFlightFences.reserve(frameCount);
	for (uint32_t frame = 0; frame < frameCount; ++frame)
	{
		imageAvailableSemaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
		computeFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
		inFlightFences.emplace_back(
			device,
			vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
	}

	recreateSwapchainImages(swapchainImageCount);
}

vk::raii::CommandPool& FrameResources::commandPool() noexcept
{
	return commandPoolHandle;
}

vk::raii::CommandBuffer& FrameResources::graphicsCommandBuffer(uint32_t frame)
{
	return graphicsCommandBuffers.at(frame);
}

vk::raii::CommandBuffer& FrameResources::computeCommandBuffer(uint32_t frame)
{
	return computeCommandBuffers.at(frame);
}

vk::raii::Semaphore& FrameResources::imageAvailableSemaphore(uint32_t frame)
{
	return imageAvailableSemaphores.at(frame);
}

vk::raii::Semaphore& FrameResources::computeFinishedSemaphore(uint32_t frame)
{
	return computeFinishedSemaphores.at(frame);
}

vk::raii::Semaphore& FrameResources::renderFinishedSemaphore(uint32_t imageIndex)
{
	return renderFinishedSemaphores.at(imageIndex);
}

vk::raii::Fence& FrameResources::inFlightFence(uint32_t frame)
{
	return inFlightFences.at(frame);
}

uint32_t FrameResources::currentFrame() const noexcept
{
	return currentFrameIndex;
}

void FrameResources::advanceFrame() noexcept
{
	currentFrameIndex = (currentFrameIndex + 1) % frameCount;
}

void FrameResources::recreateSwapchainImages(std::size_t imageCount)
{
	renderFinishedSemaphores.clear();
	renderFinishedSemaphores.reserve(imageCount);
	for (std::size_t imageIndex = 0; imageIndex < imageCount; ++imageIndex)
	{
		renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
	}
}
