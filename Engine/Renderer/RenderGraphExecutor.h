#pragma once

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

#ifndef VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#endif

#include "RenderGraph.h"

#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <functional>
#include <unordered_map>

// Executes a compiled RenderGraph on one already-recording command buffer.
//
// The graph reference and every Vulkan handle passed to BindImage are
// non-owning. The caller must keep the RenderGraph and the vk::Image objects
// alive, and must keep image views/attachments compatible with the pass
// callback, until command recording and GPU execution have completed.
class RenderGraphExecutor final
{
  public:
	using PassCallback = std::function<void(
		vk::raii::CommandBuffer& commandBuffer,
		RenderGraph::PassId pass)>;

	explicit RenderGraphExecutor(const RenderGraph& graph) noexcept;

	RenderGraphExecutor(const RenderGraphExecutor&) = delete;
	RenderGraphExecutor& operator=(const RenderGraphExecutor&) = delete;
	RenderGraphExecutor(RenderGraphExecutor&&) = delete;
	RenderGraphExecutor& operator=(RenderGraphExecutor&&) = delete;

	// Binds a non-owning vk::Image to a graph image. Rebinding the same graph
	// handle replaces its previous binding, which is useful for swapchain
	// images selected for the current frame.
	void BindImage(
		RenderGraph::ImageHandle resource,
		vk::Image image,
		vk::ImageAspectFlags aspectFlags,
		vk::ImageLayout initialLayout,
		vk::ImageLayout finalLayout);

	// Emits synchronization2 image barriers before/after pass callbacks. The
	// command buffer must already be in the recording state and is not begun or
	// ended by this class. Pass callbacks may use dynamic rendering directly.
	void Execute(
		vk::raii::CommandBuffer& commandBuffer,
		const RenderGraph::CompilationResult& compilation,
		const PassCallback& passCallback) const;

  private:
	struct ImageBinding
	{
		vk::Image Image = nullptr;
		vk::ImageAspectFlags AspectFlags{};
		vk::ImageLayout InitialLayout = vk::ImageLayout::eUndefined;
		vk::ImageLayout FinalLayout = vk::ImageLayout::eUndefined;
	};

	struct ImageState
	{
		RenderGraph::ImageHandle Resource;
		vk::PipelineStageFlags2 StageMask{};
		vk::AccessFlags2 AccessMask{};
		vk::ImageLayout Layout = vk::ImageLayout::eUndefined;
		bool HasBeenUsed = false;
		bool LastUseWrites = false;
	};

	struct UsageState
	{
		vk::PipelineStageFlags2 StageMask{};
		vk::AccessFlags2 AccessMask{};
		vk::ImageLayout Layout = vk::ImageLayout::eUndefined;
		bool Writes = false;
	};

	static UsageState TranslateUsage(
		RenderGraph::PassType passType,
		const RenderGraph::ImageUse& use,
		vk::ImageAspectFlags aspectFlags);

	static vk::PipelineStageFlags2 TranslateStage(
		RenderGraph::PassType passType,
		RenderGraph::ImageUsage usage);

	static vk::AccessFlags2 TranslateAccess(
		RenderGraph::ImageUsage usage,
		RenderGraph::AccessType access);

	static vk::ImageLayout TranslateLayout(
		RenderGraph::ImageUsage usage,
		vk::ImageAspectFlags aspectFlags);

	static bool IncludesRead(RenderGraph::AccessType access) noexcept;
	static bool IncludesWrite(RenderGraph::AccessType access) noexcept;

	void EmitImageBarrier(
		vk::raii::CommandBuffer& commandBuffer,
		const RenderGraph::ImageDescription& description,
		const ImageBinding& binding,
		vk::PipelineStageFlags2 sourceStage,
		vk::AccessFlags2 sourceAccess,
		vk::PipelineStageFlags2 destinationStage,
		vk::AccessFlags2 destinationAccess,
		vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout) const;

	const RenderGraph& Graph;
	std::unordered_map<std::uint32_t, ImageBinding> ImageBindings;
};
