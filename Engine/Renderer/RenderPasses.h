#pragma once

#include "RenderGraph.h"

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

#ifndef VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#endif

#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

class ForwardRenderer;
class GBufferResources;
class MeshRenderer;
class ParticleSystem;
class RenderTargetResources;

struct RenderPassSetup
{
	vk::Extent2D Extent{};
	std::uint32_t ShadowResolution = 0;
	vk::SampleCountFlagBits MsaaSamples = vk::SampleCountFlagBits::e1;
	std::uint64_t ParticleBufferSize = 0;
	bool DepthPrepassEnabled = false;
};

struct RenderGraphResources
{
	RenderGraph::ImageHandle ShadowMap;
	RenderGraph::ImageHandle ForwardColor;
	RenderGraph::ImageHandle ForwardDepth;
	std::array<RenderGraph::ImageHandle, 4> GBufferColors{};
	RenderGraph::ImageHandle GBufferDepth;
	RenderGraph::ImageHandle Swapchain;
	RenderGraph::BufferHandle ParticleBuffer;

	RenderGraph::PassId ParticleSimulation;
	RenderGraph::PassId ShadowPass;
	RenderGraph::PassId DepthPrepass;
	RenderGraph::PassId GeometryPass;
	RenderGraph::PassId LightingPass;
	RenderGraph::PassId ForwardPass;
	RenderGraph::PassId ParticleOverlayPass;
};

struct RenderPassContext
{
	std::uint32_t ImageIndex = 0;
	std::uint32_t FrameIndex = 0;
	vk::Extent2D Extent{};
	bool DepthPrepassEnabled = false;
	const std::vector<vk::raii::ImageView>* SwapchainImageViews = nullptr;
	RenderTargetResources* ForwardTargets = nullptr;
	GBufferResources* GBuffer = nullptr;
	MeshRenderer* Meshes = nullptr;
	ForwardRenderer* Forward = nullptr;
	ParticleSystem* Particles = nullptr;

	const vk::raii::Pipeline* ForwardPipeline = nullptr;
	const vk::raii::PipelineLayout* ForwardPipelineLayout = nullptr;
	const vk::raii::Pipeline* ShadowPipeline = nullptr;
	const vk::raii::PipelineLayout* ShadowPipelineLayout = nullptr;
	const vk::raii::Pipeline* ForwardDepthPrepassPipeline = nullptr;
	const vk::raii::Pipeline* GBufferDepthPrepassPipeline = nullptr;
	const vk::raii::Pipeline* GBufferPipeline = nullptr;
	const vk::raii::PipelineLayout* GBufferPipelineLayout = nullptr;
	const vk::raii::Pipeline* DeferredLightingPipeline = nullptr;
	const vk::raii::PipelineLayout* DeferredPipelineLayout = nullptr;
	const std::vector<vk::raii::DescriptorSet>* DeferredDescriptorSets = nullptr;
};

class RenderPass
{
  public:
	RenderPass(std::string name, RenderGraph::PassType type) noexcept;
	virtual ~RenderPass() = default;

	RenderPass(const RenderPass&) = delete;
	RenderPass& operator=(const RenderPass&) = delete;

	[[nodiscard]] const std::string& Name() const noexcept;
	[[nodiscard]] RenderGraph::PassId Id() const noexcept;

	void Setup(
		RenderGraph& graph,
		RenderGraphResources& resources,
		const RenderPassSetup& setup);
	void Record(
		vk::raii::CommandBuffer& commandBuffer,
		const RenderPassContext& context) const;

  protected:
	[[nodiscard]] RenderGraph::PassId AddPass(
		RenderGraph& graph,
		RenderGraph::PassType type = RenderGraph::PassType::Graphics);
	virtual void SetupPass(
		RenderGraph& graph,
		RenderGraphResources& resources,
		const RenderPassSetup& setup) = 0;
	virtual void RecordPass(
		vk::raii::CommandBuffer& commandBuffer,
		const RenderPassContext& context) const = 0;

	RenderGraph::PassId Pass;

  private:
	std::string PassName;
	RenderGraph::PassType PassType;
};

class RenderPassManager final
{
  public:
	RenderPassManager() = default;
	RenderPassManager(const RenderPassManager&) = delete;
	RenderPassManager& operator=(const RenderPassManager&) = delete;

	void Clear() noexcept;

	template <typename T, typename... Arguments>
	T& AddRenderPass(Arguments&&... arguments)
	{
		static_assert(std::is_base_of_v<RenderPass, T>);
		auto pass = std::make_unique<T>(std::forward<Arguments>(arguments)...);
		T& result = *pass;
		Passes.push_back(std::move(pass));
		return result;
	}

	void Setup(
		RenderGraph& graph,
		RenderGraphResources& resources,
		const RenderPassSetup& setup);
	void Record(
		vk::raii::CommandBuffer& commandBuffer,
		RenderGraph::PassId pass,
		const RenderPassContext& context) const;

  private:
	std::vector<std::unique_ptr<RenderPass>> Passes;
};

class ParticleSimulationPass final : public RenderPass
{
  public:
	ParticleSimulationPass();

  protected:
	void SetupPass(RenderGraph&, RenderGraphResources&, const RenderPassSetup&) override;
	void RecordPass(vk::raii::CommandBuffer&, const RenderPassContext&) const override;
};

class ShadowPass final : public RenderPass
{
  public:
	ShadowPass();

  protected:
	void SetupPass(RenderGraph&, RenderGraphResources&, const RenderPassSetup&) override;
	void RecordPass(vk::raii::CommandBuffer&, const RenderPassContext&) const override;
};

class DepthPrepass final : public RenderPass
{
  public:
	DepthPrepass(std::string name, bool deferred);

  protected:
	void SetupPass(RenderGraph&, RenderGraphResources&, const RenderPassSetup&) override;
	void RecordPass(vk::raii::CommandBuffer&, const RenderPassContext&) const override;

  private:
	bool Deferred = false;
};

class ForwardPass final : public RenderPass
{
  public:
	ForwardPass();

  protected:
	void SetupPass(RenderGraph&, RenderGraphResources&, const RenderPassSetup&) override;
	void RecordPass(vk::raii::CommandBuffer&, const RenderPassContext&) const override;
};

class GeometryPass final : public RenderPass
{
  public:
	GeometryPass();

  protected:
	void SetupPass(RenderGraph&, RenderGraphResources&, const RenderPassSetup&) override;
	void RecordPass(vk::raii::CommandBuffer&, const RenderPassContext&) const override;
};

class LightingPass final : public RenderPass
{
  public:
	LightingPass();

  protected:
	void SetupPass(RenderGraph&, RenderGraphResources&, const RenderPassSetup&) override;
	void RecordPass(vk::raii::CommandBuffer&, const RenderPassContext&) const override;
};

class ParticleOverlayPass final : public RenderPass
{
  public:
	ParticleOverlayPass();

  protected:
	void SetupPass(RenderGraph&, RenderGraphResources&, const RenderPassSetup&) override;
	void RecordPass(vk::raii::CommandBuffer&, const RenderPassContext&) const override;
};
