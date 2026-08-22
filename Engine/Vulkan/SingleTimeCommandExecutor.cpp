#include "SingleTimeCommandExecutor.h"

SingleTimeCommandExecutor::SingleTimeCommandExecutor(
	VulkanContext& vulkan,
	vk::raii::CommandPool& commandPool) noexcept
	: Vulkan(vulkan), CommandPool(commandPool)
{
}

vk::raii::CommandBuffer SingleTimeCommandExecutor::Begin() const
{
	const vk::CommandBufferAllocateInfo allocateInfo{
		.commandPool = CommandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1};
	vk::raii::CommandBuffer commandBuffer = std::move(
		vk::raii::CommandBuffers(Vulkan.device(), allocateInfo).front());
	const vk::CommandBufferBeginInfo beginInfo{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
	commandBuffer.begin(beginInfo);
	return std::move(commandBuffer);
}

void SingleTimeCommandExecutor::End(vk::raii::CommandBuffer&& commandBuffer) const
{
	commandBuffer.end();
	const vk::SubmitInfo submitInfo{
		.commandBufferCount = 1,
		.pCommandBuffers = &*commandBuffer};
	Vulkan.queue().submit(submitInfo, nullptr);
	Vulkan.queue().waitIdle();
}

void SingleTimeCommandExecutor::CopyBuffer(
	vk::raii::Buffer& source,
	vk::raii::Buffer& destination,
	vk::DeviceSize size) const
{
	vk::raii::CommandBuffer commandBuffer = Begin();
	commandBuffer.copyBuffer(*source, *destination, vk::BufferCopy{.size = size});
	End(std::move(commandBuffer));
}
