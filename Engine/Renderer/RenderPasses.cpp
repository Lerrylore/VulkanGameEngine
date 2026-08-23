#include "RenderPasses.h"

#include "ForwardRenderer.h"
#include "GBufferResources.h"
#include "MeshRenderer.h"
#include "ParticleSystem.h"
#include "RenderTargetResources.h"

#include <array>
#include <stdexcept>
#include <utility>

namespace
{
	void Require(const void* pointer, const char* name)
	{
		if (pointer == nullptr)
		{
			throw std::invalid_argument(std::string("Render pass requires ") + name);
		}
	}

	void SetViewportAndScissor(
		vk::raii::CommandBuffer& commandBuffer,
		vk::Extent2D extent)
	{
		commandBuffer.setViewport(
			0,
			vk::Viewport(
				0.0f,
				0.0f,
				static_cast<float>(extent.width),
				static_cast<float>(extent.height),
				0.0f,
				1.0f));
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
	}

	void RecordDepthOnly(
		vk::raii::CommandBuffer& commandBuffer,
		const RenderPassContext& context,
		const vk::raii::ImageView& depthView,
		const vk::raii::Pipeline& pipeline,
		const vk::raii::PipelineLayout& pipelineLayout)
	{
		const vk::RenderingAttachmentInfo depthAttachment{
			.imageView = depthView,
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearDepthStencilValue(1.0f, 0)};
		const vk::RenderingInfo renderingInfo{
			.renderArea = {.offset = {0, 0}, .extent = context.Extent},
			.layerCount = 1,
			.colorAttachmentCount = 0,
			.pDepthAttachment = &depthAttachment};

		commandBuffer.beginRendering(renderingInfo);
		SetViewportAndScissor(commandBuffer, context.Extent);
		context.Meshes->RecordDraws(commandBuffer, pipeline, pipelineLayout, context.FrameIndex);
		commandBuffer.endRendering();
	}
}

RenderPass::RenderPass(std::string name, RenderGraph::PassType type) noexcept
	: PassName(std::move(name)), PassType(type)
{
}

const std::string& RenderPass::Name() const noexcept
{
	return PassName;
}

RenderGraph::PassId RenderPass::Id() const noexcept
{
	return Pass;
}

void RenderPass::Setup(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const RenderPassSetup& setup)
{
	Pass = AddPass(graph, PassType);
	SetupPass(graph, resources, setup);
}

void RenderPass::Record(
	vk::raii::CommandBuffer& commandBuffer,
	const RenderPassContext& context) const
{
	RecordPass(commandBuffer, context);
}

RenderGraph::PassId RenderPass::AddPass(
	RenderGraph& graph,
	RenderGraph::PassType type)
{
	return graph.AddPass(PassName, type);
}

void RenderPassManager::Clear() noexcept
{
	Passes.clear();
}

void RenderPassManager::Setup(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const RenderPassSetup& setup)
{
	for (const auto& pass : Passes)
	{
		pass->Setup(graph, resources, setup);
	}
}

void RenderPassManager::Record(
	vk::raii::CommandBuffer& commandBuffer,
	RenderGraph::PassId pass,
	const RenderPassContext& context) const
{
	for (const auto& renderPass : Passes)
	{
		if (renderPass->Id() == pass)
		{
			renderPass->Record(commandBuffer, context);
			return;
		}
	}
	throw std::invalid_argument("RenderPassManager received an unknown graph pass");
}

ParticleSimulationPass::ParticleSimulationPass()
	: RenderPass("ParticleSimulation", RenderGraph::PassType::Compute)
{
}

void ParticleSimulationPass::SetupPass(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const RenderPassSetup&)
{
	resources.ParticleSimulation = Pass;
	graph.Write(Pass, resources.ParticleBuffer, RenderGraph::BufferUsage::Storage);
}

void ParticleSimulationPass::RecordPass(
	vk::raii::CommandBuffer& commandBuffer,
	const RenderPassContext& context) const
{
	Require(context.Particles, "ParticleSystem");
	context.Particles->RecordComputeCommands(commandBuffer, context.FrameIndex);
}

ShadowPass::ShadowPass()
	: RenderPass("ShadowPass", RenderGraph::PassType::Graphics)
{
}

void ShadowPass::SetupPass(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const RenderPassSetup&)
{
	resources.ShadowPass = Pass;
	graph.ReadWrite(Pass, resources.ShadowMap, RenderGraph::ImageUsage::DepthAttachment);
}

void ShadowPass::RecordPass(
	vk::raii::CommandBuffer& commandBuffer,
	const RenderPassContext& context) const
{
	Require(context.Forward, "ForwardRenderer");
	Require(context.ShadowPipeline, "shadow pipeline");
	Require(context.ShadowPipelineLayout, "shadow pipeline layout");
	context.Forward->RecordShadowPassContents(
		commandBuffer,
		context.FrameIndex,
		*context.ShadowPipeline,
		*context.ShadowPipelineLayout);
}

DepthPrepass::DepthPrepass(std::string name, bool deferred)
	: RenderPass(std::move(name), RenderGraph::PassType::Graphics), Deferred(deferred)
{
}

void DepthPrepass::SetupPass(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const RenderPassSetup&)
{
	resources.DepthPrepass = Pass;
	graph.ReadWrite(
		Pass,
		Deferred ? resources.GBufferDepth : resources.ForwardDepth,
		RenderGraph::ImageUsage::DepthAttachment);
}

void DepthPrepass::RecordPass(
	vk::raii::CommandBuffer& commandBuffer,
	const RenderPassContext& context) const
{
	Require(context.Meshes, "MeshRenderer");
	if (Deferred)
	{
		Require(context.GBuffer, "GBufferResources");
		Require(context.GBufferDepthPrepassPipeline, "G-buffer depth prepass pipeline");
		Require(context.ForwardPipelineLayout, "depth prepass pipeline layout");
		RecordDepthOnly(
			commandBuffer,
			context,
			context.GBuffer->DepthView(context.FrameIndex),
			*context.GBufferDepthPrepassPipeline,
			*context.ForwardPipelineLayout);
		return;
	}

	Require(context.ForwardTargets, "forward render targets");
	Require(context.ForwardDepthPrepassPipeline, "forward depth prepass pipeline");
	Require(context.ForwardPipelineLayout, "depth prepass pipeline layout");
	RecordDepthOnly(
		commandBuffer,
		context,
		context.ForwardTargets->depthImageView(),
		*context.ForwardDepthPrepassPipeline,
		*context.ForwardPipelineLayout);
}

ForwardPass::ForwardPass()
	: RenderPass("ForwardOpaquePass", RenderGraph::PassType::Graphics)
{
}

void ForwardPass::SetupPass(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const RenderPassSetup& setup)
{
	resources.ForwardPass = Pass;
	graph.Read(Pass, resources.ShadowMap, RenderGraph::ImageUsage::Sampled);
	graph.Write(Pass, resources.ForwardColor, RenderGraph::ImageUsage::ColorAttachment);
	if (setup.DepthPrepassEnabled)
	{
		graph.Read(Pass, resources.ForwardDepth, RenderGraph::ImageUsage::DepthAttachment);
	}
	else
	{
		graph.ReadWrite(Pass, resources.ForwardDepth, RenderGraph::ImageUsage::DepthAttachment);
	}
	graph.Write(Pass, resources.Swapchain, RenderGraph::ImageUsage::ColorAttachment);
}

void ForwardPass::RecordPass(
	vk::raii::CommandBuffer& commandBuffer,
	const RenderPassContext& context) const
{
	Require(context.Forward, "ForwardRenderer");
	Require(context.ForwardPipeline, "forward pipeline");
	Require(context.ForwardPipelineLayout, "forward pipeline layout");
	context.Forward->RecordForwardPassContents(
		commandBuffer,
		context.ImageIndex,
		context.FrameIndex,
		*context.ForwardPipeline,
		*context.ForwardPipelineLayout,
		nullptr,
		context.DepthPrepassEnabled);
}

GeometryPass::GeometryPass()
	: RenderPass("GBufferPass", RenderGraph::PassType::Graphics)
{
}

void GeometryPass::SetupPass(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const RenderPassSetup& setup)
{
	resources.GeometryPass = Pass;
	for (const auto resource : resources.GBufferColors)
	{
		graph.Write(Pass, resource, RenderGraph::ImageUsage::ColorAttachment);
	}
	if (setup.DepthPrepassEnabled)
	{
		graph.Read(Pass, resources.GBufferDepth, RenderGraph::ImageUsage::DepthAttachment);
	}
	else
	{
		graph.ReadWrite(Pass, resources.GBufferDepth, RenderGraph::ImageUsage::DepthAttachment);
	}
}

void GeometryPass::RecordPass(
	vk::raii::CommandBuffer& commandBuffer,
	const RenderPassContext& context) const
{
	Require(context.GBuffer, "GBufferResources");
	Require(context.Meshes, "MeshRenderer");
	Require(context.GBufferPipeline, "G-buffer pipeline");
	Require(context.GBufferPipelineLayout, "G-buffer pipeline layout");

	std::array<vk::RenderingAttachmentInfo, GBufferResources::AttachmentCount> attachments{};
	for (std::size_t attachment = 0; attachment < attachments.size(); ++attachment)
	{
		attachments[attachment] = vk::RenderingAttachmentInfo{
			.imageView = context.GBuffer->ColorView(context.FrameIndex, attachment),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f)};
	}
	const vk::RenderingAttachmentInfo depthAttachment{
		.imageView = context.GBuffer->DepthView(context.FrameIndex),
		.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		.loadOp = context.DepthPrepassEnabled ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearDepthStencilValue(1.0f, 0)};
	const vk::RenderingInfo renderingInfo{
		.renderArea = {.offset = {0, 0}, .extent = context.Extent},
		.layerCount = 1,
		.colorAttachmentCount = static_cast<std::uint32_t>(attachments.size()),
		.pColorAttachments = attachments.data(),
		.pDepthAttachment = &depthAttachment};

	commandBuffer.beginRendering(renderingInfo);
	SetViewportAndScissor(commandBuffer, context.Extent);
	context.Meshes->RecordDraws(
		commandBuffer,
		*context.GBufferPipeline,
		*context.GBufferPipelineLayout,
		context.FrameIndex);
	commandBuffer.endRendering();
}

LightingPass::LightingPass()
	: RenderPass("DeferredLightingPass", RenderGraph::PassType::Graphics)
{
}

void LightingPass::SetupPass(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const RenderPassSetup&)
{
	resources.LightingPass = Pass;
	graph.Read(Pass, resources.ShadowMap, RenderGraph::ImageUsage::Sampled);
	for (const auto resource : resources.GBufferColors)
	{
		graph.Read(Pass, resource, RenderGraph::ImageUsage::Sampled);
	}
	graph.Write(Pass, resources.Swapchain, RenderGraph::ImageUsage::ColorAttachment);
}

void LightingPass::RecordPass(
	vk::raii::CommandBuffer& commandBuffer,
	const RenderPassContext& context) const
{
	Require(context.SwapchainImageViews, "swapchain image views");
	Require(context.DeferredLightingPipeline, "deferred lighting pipeline");
	Require(context.DeferredPipelineLayout, "deferred pipeline layout");
	Require(context.DeferredDescriptorSets, "deferred descriptor sets");

	const vk::RenderingAttachmentInfo colorAttachment{
		.imageView = (*context.SwapchainImageViews)[context.ImageIndex],
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)};
	const vk::RenderingInfo renderingInfo{
		.renderArea = {.offset = {0, 0}, .extent = context.Extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachment};

	commandBuffer.beginRendering(renderingInfo);
	SetViewportAndScissor(commandBuffer, context.Extent);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, **context.DeferredLightingPipeline);
	commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		**context.DeferredPipelineLayout,
		0,
		*(*context.DeferredDescriptorSets)[context.FrameIndex],
		nullptr);
	commandBuffer.draw(3, 1, 0, 0);
	commandBuffer.endRendering();
}

ParticleOverlayPass::ParticleOverlayPass()
	: RenderPass("ParticleOverlayPass", RenderGraph::PassType::Graphics)
{
}

void ParticleOverlayPass::SetupPass(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const RenderPassSetup&)
{
	resources.ParticleOverlayPass = Pass;
	graph.ReadWrite(Pass, resources.Swapchain, RenderGraph::ImageUsage::ColorAttachment);
	graph.Read(Pass, resources.ParticleBuffer, RenderGraph::BufferUsage::Vertex);
}

void ParticleOverlayPass::RecordPass(
	vk::raii::CommandBuffer& commandBuffer,
	const RenderPassContext& context) const
{
	Require(context.SwapchainImageViews, "swapchain image views");
	Require(context.Particles, "ParticleSystem");

	const vk::RenderingAttachmentInfo colorAttachment{
		.imageView = (*context.SwapchainImageViews)[context.ImageIndex],
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eLoad,
		.storeOp = vk::AttachmentStoreOp::eStore};
	const vk::RenderingInfo renderingInfo{
		.renderArea = {.offset = {0, 0}, .extent = context.Extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachment};

	commandBuffer.beginRendering(renderingInfo);
	SetViewportAndScissor(commandBuffer, context.Extent);
	context.Particles->RecordDraw(commandBuffer, context.FrameIndex);
	commandBuffer.endRendering();
}
