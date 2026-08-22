#pragma once

#include "../Vulkan/VulkanContext.h"

#include <functional>

class MeshRenderer;
class RenderTargetResources;
class ShadowMapResources;
class SwapchainResources;

class ForwardRenderer final
{
  public:
	using ParticleDrawCallback = std::function<void(vk::raii::CommandBuffer&, uint32_t)>;

	ForwardRenderer(
		const SwapchainResources& swapchain,
		RenderTargetResources& renderTargets,
		ShadowMapResources& shadowMaps,
		MeshRenderer& meshRenderer) noexcept;

	ForwardRenderer(const ForwardRenderer&) = delete;
	ForwardRenderer& operator=(const ForwardRenderer&) = delete;
	ForwardRenderer(ForwardRenderer&&) = delete;
	ForwardRenderer& operator=(ForwardRenderer&&) = delete;

	void Record(
		vk::raii::CommandBuffer& commandBuffer,
		uint32_t imageIndex,
		uint32_t frameIndex,
		const vk::raii::Pipeline& graphicsPipeline,
		const vk::raii::PipelineLayout& graphicsPipelineLayout,
		const vk::raii::Pipeline& shadowPipeline,
		const vk::raii::PipelineLayout& shadowPipelineLayout,
		ParticleDrawCallback particleDraw);

	// Graph callbacks: these functions record only pass contents. Image layout
	// transitions are owned by RenderGraphExecutor.
	void RecordShadowPassContents(
		vk::raii::CommandBuffer& commandBuffer,
		uint32_t frameIndex,
		const vk::raii::Pipeline& shadowPipeline,
		const vk::raii::PipelineLayout& shadowPipelineLayout);

	void RecordForwardPassContents(
		vk::raii::CommandBuffer& commandBuffer,
		uint32_t imageIndex,
		uint32_t frameIndex,
		const vk::raii::Pipeline& graphicsPipeline,
		const vk::raii::PipelineLayout& graphicsPipelineLayout,
		ParticleDrawCallback particleDraw);

  private:
	void RecordShadowPass(
		vk::raii::CommandBuffer& commandBuffer,
		uint32_t frameIndex,
		const vk::raii::Pipeline& shadowPipeline,
		const vk::raii::PipelineLayout& shadowPipelineLayout);

	void TransitionImageLayout(
		vk::raii::CommandBuffer& commandBuffer,
		vk::Image image,
		vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout,
		vk::AccessFlags2 sourceAccess,
		vk::AccessFlags2 destinationAccess,
		vk::PipelineStageFlags2 sourceStage,
		vk::PipelineStageFlags2 destinationStage,
		vk::ImageAspectFlags aspectFlags) const;

	const SwapchainResources& Swapchain;
	RenderTargetResources& RenderTargets;
	ShadowMapResources& ShadowMaps;
	MeshRenderer& Meshes;
};
