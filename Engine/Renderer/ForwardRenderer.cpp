#include "ForwardRenderer.h"

#include "MeshRenderer.h"
#include "RenderTargetResources.h"
#include "ShadowMapResources.h"
#include "SwapchainResources.h"

#include <array>
#include <utility>

ForwardRenderer::ForwardRenderer(
	const SwapchainResources& swapchain,
	RenderTargetResources& renderTargets,
	ShadowMapResources& shadowMaps,
	MeshRenderer& meshRenderer) noexcept
	: Swapchain(swapchain),
	  RenderTargets(renderTargets),
	  ShadowMaps(shadowMaps),
	  Meshes(meshRenderer)
{
}

void ForwardRenderer::Record(
	vk::raii::CommandBuffer& commandBuffer,
	uint32_t imageIndex,
	uint32_t frameIndex,
	const vk::raii::Pipeline& graphicsPipeline,
	const vk::raii::PipelineLayout& graphicsPipelineLayout,
	const vk::raii::Pipeline& shadowPipeline,
	const vk::raii::PipelineLayout& shadowPipelineLayout,
	ParticleDrawCallback particleDraw)
{
	commandBuffer.begin({});
	RecordShadowPass(commandBuffer, frameIndex, shadowPipeline, shadowPipelineLayout);

	TransitionImageLayout(
		commandBuffer,
		Swapchain.images()[imageIndex],
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::ImageAspectFlagBits::eColor);
	TransitionImageLayout(
		commandBuffer,
		*RenderTargets.colorImage(),
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::ImageAspectFlagBits::eColor);
	TransitionImageLayout(
		commandBuffer,
		*RenderTargets.depthImage(),
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eDepthAttachmentOptimal,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::ImageAspectFlagBits::eDepth);

	const vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
	const vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
	const vk::RenderingAttachmentInfo colorAttachment{
		.imageView = RenderTargets.colorImageView(),
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.resolveMode = vk::ResolveModeFlagBits::eAverage,
		.resolveImageView = Swapchain.imageViews()[imageIndex],
		.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = clearColor};
	const vk::RenderingAttachmentInfo depthAttachment{
		.imageView = RenderTargets.depthImageView(),
		.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eDontCare,
		.clearValue = clearDepth};
	const vk::RenderingInfo renderingInfo{
		.renderArea = {.offset = {0, 0}, .extent = Swapchain.extent()},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachment,
		.pDepthAttachment = &depthAttachment};

	commandBuffer.beginRendering(renderingInfo);
	commandBuffer.setViewport(
		0,
		vk::Viewport(
			0.0f,
			0.0f,
			static_cast<float>(Swapchain.extent().width),
			static_cast<float>(Swapchain.extent().height),
			0.0f,
			1.0f));
	commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), Swapchain.extent()));
	Meshes.RecordDraws(commandBuffer, graphicsPipeline, graphicsPipelineLayout, frameIndex);
	if (particleDraw)
	{
		particleDraw(commandBuffer, frameIndex);
	}
	commandBuffer.endRendering();

	TransitionImageLayout(
		commandBuffer,
		Swapchain.images()[imageIndex],
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::ePresentSrcKHR,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		{},
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eBottomOfPipe,
		vk::ImageAspectFlagBits::eColor);
	commandBuffer.end();
}

void ForwardRenderer::RecordShadowPass(
	vk::raii::CommandBuffer& commandBuffer,
	uint32_t frameIndex,
	const vk::raii::Pipeline& shadowPipeline,
	const vk::raii::PipelineLayout& shadowPipelineLayout)
{
	const vk::raii::Image& shadowImage = ShadowMaps.GetImage(frameIndex);
	TransitionImageLayout(
		commandBuffer,
		*shadowImage,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eDepthAttachmentOptimal,
		{},
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::PipelineStageFlagBits2::eTopOfPipe,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::ImageAspectFlagBits::eDepth);
	RecordShadowPassContents(commandBuffer, frameIndex, shadowPipeline, shadowPipelineLayout);
	TransitionImageLayout(
		commandBuffer,
		*shadowImage,
		vk::ImageLayout::eDepthAttachmentOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::AccessFlagBits2::eShaderSampledRead,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::ImageAspectFlagBits::eDepth);
}

void ForwardRenderer::RecordShadowPassContents(
	vk::raii::CommandBuffer& commandBuffer,
	uint32_t frameIndex,
	const vk::raii::Pipeline& shadowPipeline,
	const vk::raii::PipelineLayout& shadowPipelineLayout)
{
	const vk::Extent2D shadowExtent{ShadowMaps.GetResolution(), ShadowMaps.GetResolution()};

	const vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
	const vk::RenderingAttachmentInfo depthAttachment{
		.imageView = ShadowMaps.GetImageView(frameIndex),
		.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = clearDepth};
	const vk::RenderingInfo renderingInfo{
		.renderArea = {.offset = {0, 0}, .extent = shadowExtent},
		.layerCount = 1,
		.colorAttachmentCount = 0,
		.pDepthAttachment = &depthAttachment};

	commandBuffer.beginRendering(renderingInfo);
	commandBuffer.setViewport(
		0,
		vk::Viewport(
			0.0f,
			0.0f,
			static_cast<float>(shadowExtent.width),
			static_cast<float>(shadowExtent.height),
			0.0f,
			1.0f));
	commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), shadowExtent));
	Meshes.RecordShadowDraws(commandBuffer, shadowPipeline, shadowPipelineLayout, frameIndex);
	commandBuffer.endRendering();
}

void ForwardRenderer::RecordForwardPassContents(
	vk::raii::CommandBuffer& commandBuffer,
	uint32_t imageIndex,
	uint32_t frameIndex,
	const vk::raii::Pipeline& graphicsPipeline,
	const vk::raii::PipelineLayout& graphicsPipelineLayout,
	ParticleDrawCallback particleDraw,
	bool loadDepth)
{
	const vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
	const vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
	const vk::RenderingAttachmentInfo colorAttachment{
		.imageView = RenderTargets.colorImageView(),
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.resolveMode = vk::ResolveModeFlagBits::eAverage,
		.resolveImageView = Swapchain.imageViews()[imageIndex],
		.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = clearColor};
	const vk::RenderingAttachmentInfo depthAttachment{
		.imageView = RenderTargets.depthImageView(),
		.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		.loadOp = loadDepth ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eDontCare,
		.clearValue = clearDepth};
	const vk::RenderingInfo renderingInfo{
		.renderArea = {.offset = {0, 0}, .extent = Swapchain.extent()},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachment,
		.pDepthAttachment = &depthAttachment};

	commandBuffer.beginRendering(renderingInfo);
	commandBuffer.setViewport(
		0,
		vk::Viewport(
			0.0f,
			0.0f,
			static_cast<float>(Swapchain.extent().width),
			static_cast<float>(Swapchain.extent().height),
			0.0f,
			1.0f));
	commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), Swapchain.extent()));
	Meshes.RecordDraws(commandBuffer, graphicsPipeline, graphicsPipelineLayout, frameIndex);
	if (particleDraw)
	{
		particleDraw(commandBuffer, frameIndex);
	}
	commandBuffer.endRendering();
}

void ForwardRenderer::TransitionImageLayout(
	vk::raii::CommandBuffer& commandBuffer,
	vk::Image image,
	vk::ImageLayout oldLayout,
	vk::ImageLayout newLayout,
	vk::AccessFlags2 sourceAccess,
	vk::AccessFlags2 destinationAccess,
	vk::PipelineStageFlags2 sourceStage,
	vk::PipelineStageFlags2 destinationStage,
	vk::ImageAspectFlags aspectFlags) const
{
	const vk::ImageMemoryBarrier2 barrier{
		.srcStageMask = sourceStage,
		.srcAccessMask = sourceAccess,
		.dstStageMask = destinationStage,
		.dstAccessMask = destinationAccess,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = {
			.aspectMask = aspectFlags,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1}};
	const vk::DependencyInfo dependencyInfo{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier};
	commandBuffer.pipelineBarrier2(dependencyInfo);
}
