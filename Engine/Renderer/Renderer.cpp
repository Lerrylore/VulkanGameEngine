#include "Renderer.h"

#include <stdexcept>

void Renderer::SetRenderPath(RenderPath path) noexcept
{
	CurrentPath = path;
}

Renderer::RenderPath Renderer::GetRenderPath() const noexcept
{
	return CurrentPath;
}

Renderer::FrameGraph Renderer::BuildFrameGraph(
	const FrameDescription& description)
{
	if (description.Extent.width == 0 || description.Extent.height == 0)
	{
		throw std::invalid_argument("Renderer requires a non-zero frame extent");
	}
	if (description.ShadowResolution == 0 || description.ParticleBufferSize == 0)
	{
		throw std::invalid_argument(
			"Renderer requires a shadow resolution and particle buffer size");
	}

	FrameGraph frameGraph;
	const RenderPassSetup setup{
		.Extent = description.Extent,
		.ShadowResolution = description.ShadowResolution,
		.MsaaSamples = description.MsaaSamples,
		.ParticleBufferSize = description.ParticleBufferSize,
		.DepthPrepassEnabled = description.DepthPrepassEnabled};
	SetupRenderPasses(frameGraph.Graph, frameGraph.Resources, description, setup);
	return frameGraph;
}

void Renderer::SetupRenderPasses(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const FrameDescription& description,
	const RenderPassSetup& setup)
{
	PassManager.Clear();
	SetupCommonResources(graph, resources, description, setup);

	PassManager.AddRenderPass<ParticleSimulationPass>();
	PassManager.AddRenderPass<ShadowPass>();
	if (setup.DepthPrepassEnabled)
	{
		PassManager.AddRenderPass<DepthPrepass>(
			CurrentPath == RenderPath::Deferred ? "GBufferDepthPrepass" : "ForwardDepthPrepass",
			CurrentPath == RenderPath::Deferred);
	}

	if (CurrentPath == RenderPath::Deferred)
	{
		SetupDeferredRenderer(graph, resources, description, setup);
	}
	else
	{
		SetupForwardRenderer(graph, resources, description, setup);
	}

	PassManager.AddRenderPass<ParticleOverlayPass>();
	PassManager.Setup(graph, resources, setup);
}

void Renderer::SetupCommonResources(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const FrameDescription& description,
	const RenderPassSetup& setup) const
{
	// This is deliberately the same information as the guide's AddResource:
	// format, Vulkan capability flags, and the external layout contract.
	resources.ShadowMap = graph.AddResource(
		"ShadowMap",
		{
			.Format = description.ShadowFormat,
			.Extent = {setup.ShadowResolution, setup.ShadowResolution},
			.Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
				vk::ImageUsageFlagBits::eSampled,
			.InitialLayout = vk::ImageLayout::eUndefined,
			.FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.Imported = true,
			.Transient = false});
	resources.Swapchain = graph.AddResource(
		"Swapchain",
		{
			.Format = description.SwapchainFormat,
			.Extent = setup.Extent,
			.Usage = vk::ImageUsageFlagBits::eColorAttachment,
			.InitialLayout = vk::ImageLayout::eUndefined,
			.FinalLayout = vk::ImageLayout::ePresentSrcKHR,
			.Imported = true,
			.Transient = false});
	resources.ParticleBuffer = graph.AddBuffer(
		"ParticleBuffer",
		{.Size = setup.ParticleBufferSize, .Imported = true, .Transient = false});
}

void Renderer::SetupForwardRenderer(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const FrameDescription& description,
	const RenderPassSetup& setup)
{
	resources.ForwardColor = graph.AddResource(
		"ForwardColorMSAA",
		{
			.Format = description.ForwardColorFormat,
			.Extent = setup.Extent,
			.Usage = vk::ImageUsageFlagBits::eTransientAttachment |
				vk::ImageUsageFlagBits::eColorAttachment,
			.InitialLayout = vk::ImageLayout::eUndefined,
			.FinalLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.Samples = setup.MsaaSamples,
			.Imported = true,
			.Transient = false});
	resources.ForwardDepth = graph.AddResource(
		"ForwardDepthMSAA",
		{
			.Format = description.ForwardDepthFormat,
			.Extent = setup.Extent,
			.Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
			.InitialLayout = vk::ImageLayout::eUndefined,
			.FinalLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.Samples = setup.MsaaSamples,
			.Imported = true,
			.Transient = false});
	PassManager.AddRenderPass<ForwardPass>();
}

void Renderer::SetupDeferredRenderer(
	RenderGraph& graph,
	RenderGraphResources& resources,
	const FrameDescription& description,
	const RenderPassSetup& setup)
{
	// The lighting pass reads these through sampled descriptors. Therefore
	// eSampled (not eInputAttachment) is the capability matching this dynamic
	// rendering implementation.
	resources.GBufferColors = {
		graph.AddResource("GBufferAlbedoMetallic", {
			.Format = description.GBufferFormats[0], .Extent = setup.Extent,
			.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			.InitialLayout = vk::ImageLayout::eUndefined,
			.FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.Imported = true, .Transient = false}),
		graph.AddResource("GBufferNormalRoughness", {
			.Format = description.GBufferFormats[1], .Extent = setup.Extent,
			.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			.InitialLayout = vk::ImageLayout::eUndefined,
			.FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.Imported = true, .Transient = false}),
		graph.AddResource("GBufferWorldPosition", {
			.Format = description.GBufferFormats[2], .Extent = setup.Extent,
			.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			.InitialLayout = vk::ImageLayout::eUndefined,
			.FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.Imported = true, .Transient = false}),
		graph.AddResource("GBufferEmissiveOcclusion", {
			.Format = description.GBufferFormats[3], .Extent = setup.Extent,
			.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			.InitialLayout = vk::ImageLayout::eUndefined,
			.FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.Imported = true, .Transient = false})};
	resources.GBufferDepth = graph.AddResource(
		"GBufferDepth",
		{
			.Format = description.GBufferDepthFormat,
			.Extent = setup.Extent,
			.Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
				vk::ImageUsageFlagBits::eSampled,
			.InitialLayout = vk::ImageLayout::eUndefined,
			.FinalLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.Imported = true,
			.Transient = false});
	PassManager.AddRenderPass<GeometryPass>();
	PassManager.AddRenderPass<LightingPass>();
}

void Renderer::RecordPass(
	vk::raii::CommandBuffer& commandBuffer,
	RenderGraph::PassId pass,
	const RenderPassContext& context) const
{
	PassManager.Record(commandBuffer, pass, context);
}
