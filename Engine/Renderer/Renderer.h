#pragma once

#include "RenderGraph.h"
#include "RenderPasses.h"

#include <array>
#include <cstdint>

// Frame-level renderer described by the tutorial. Renderer selects the active
// technique, composes its RenderPass objects, and exposes the resulting graph
// to the application for resource binding and command submission.
class Renderer final
{
  public:
	enum class RenderPath
	{
		Forward,
		Deferred
	};

	struct FrameDescription
	{
		vk::Extent2D Extent{};
		std::uint32_t ShadowResolution = 0;
		vk::SampleCountFlagBits MsaaSamples = vk::SampleCountFlagBits::e1;
		std::uint64_t ParticleBufferSize = 0;
		bool DepthPrepassEnabled = false;
		vk::Format ShadowFormat = vk::Format::eUndefined;
		vk::Format SwapchainFormat = vk::Format::eUndefined;
		vk::Format ForwardColorFormat = vk::Format::eUndefined;
		vk::Format ForwardDepthFormat = vk::Format::eUndefined;
		std::array<vk::Format, 4> GBufferFormats{};
		vk::Format GBufferDepthFormat = vk::Format::eUndefined;
	};

	struct FrameGraph
	{
		RenderGraph Graph;
		RenderGraphResources Resources;
	};

	void SetRenderPath(RenderPath path) noexcept;
	[[nodiscard]] RenderPath GetRenderPath() const noexcept;
	[[nodiscard]] FrameGraph BuildFrameGraph(const FrameDescription& description);

	void RecordPass(
		vk::raii::CommandBuffer& commandBuffer,
		RenderGraph::PassId pass,
		const RenderPassContext& context) const;

  private:
	// This is the tutorial's Renderer::SetupRenderPasses(). The graph is the
	// modern RenderPassManager: it owns declarations, ordering and barriers;
	// derived RenderPass classes own dynamic-rendering recording.
	void SetupRenderPasses(
		RenderGraph& graph,
		RenderGraphResources& resources,
		const FrameDescription& description,
		const RenderPassSetup& setup);

	void SetupCommonResources(
		RenderGraph& graph,
		RenderGraphResources& resources,
		const FrameDescription& description,
		const RenderPassSetup& setup) const;
	void SetupForwardRenderer(
		RenderGraph& graph,
		RenderGraphResources& resources,
		const FrameDescription& description,
		const RenderPassSetup& setup);
	void SetupDeferredRenderer(
		RenderGraph& graph,
		RenderGraphResources& resources,
		const FrameDescription& description,
		const RenderPassSetup& setup);

	RenderPath CurrentPath = RenderPath::Forward;
	RenderPassManager PassManager;
};
