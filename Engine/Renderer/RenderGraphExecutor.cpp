#include "RenderGraphExecutor.h"

#include <stdexcept>

RenderGraphExecutor::RenderGraphExecutor(const RenderGraph& graph) noexcept
	: Graph(graph)
{
}

void RenderGraphExecutor::BindImage(
	RenderGraph::ImageHandle resource,
	vk::Image image,
	vk::ImageAspectFlags aspectFlags)
{
	// GetImageDescription also validates that the handle belongs to Graph. We
	// intentionally do not retain a pointer to the graph's image record.
	const auto& description = Graph.GetImageDescription(resource);
	if (!resource.IsValid() || image == nullptr || !aspectFlags)
	{
		throw std::invalid_argument("RenderGraphExecutor image binding is invalid");
	}
	if (description.MipLevels == 0 || description.Layers == 0)
	{
		throw std::invalid_argument(
			"RenderGraphExecutor image binding has an empty subresource range");
	}
	if (description.FinalLayout == vk::ImageLayout::eUndefined)
	{
		throw std::invalid_argument(
			"RenderGraphExecutor final image layout cannot be eUndefined");
	}

	ImageBindings[resource.Index] = ImageBinding{
		.Image = image,
		.AspectFlags = aspectFlags};
}

void RenderGraphExecutor::BindBuffer(
	RenderGraph::BufferHandle resource,
	vk::Buffer buffer)
{
	[[maybe_unused]] const auto& description = Graph.GetBufferDescription(resource);
	if (!resource.IsValid() || buffer == nullptr)
	{
		throw std::invalid_argument("RenderGraphExecutor buffer binding is invalid");
	}

	BufferBindings[resource.Index] = BufferBinding{.Buffer = buffer};
}

void RenderGraphExecutor::Execute(
	vk::raii::CommandBuffer& commandBuffer,
	const RenderGraph::CompilationResult& compilation,
	const PassCallback& passCallback) const
{
	if (!compilation.Succeeded)
	{
		throw std::invalid_argument(
			"RenderGraphExecutor cannot execute an invalid RenderGraph compilation");
	}
	if (!passCallback)
	{
		throw std::invalid_argument("RenderGraphExecutor requires a pass callback");
	}

	// Validate the complete recording plan before emitting the first barrier.
	// This prevents a missing binding in a later pass from leaving a partially
	// recorded command buffer as the observable failure mode.
	for (const auto pass : compilation.ExecutionOrder)
	{
		const auto& passDescription = Graph.GetPassDescription(pass);
		for (const auto& use : passDescription.Uses)
		{
			const auto binding = ImageBindings.find(use.Resource.Index);
			if (binding == ImageBindings.end())
			{
				throw std::invalid_argument(
					"RenderGraphExecutor has no Vulkan image binding for resource '" +
					Graph.GetImageName(use.Resource) + "'");
			}
			TranslateUsage(passDescription.Type, use, binding->second.AspectFlags);
		}
		for (const auto& use : passDescription.BufferUses)
		{
			if (BufferBindings.find(use.Resource.Index) == BufferBindings.end())
			{
				throw std::invalid_argument(
					"RenderGraphExecutor has no Vulkan buffer binding for resource '" +
					Graph.GetBufferName(use.Resource) + "'");
			}
			TranslateUsage(passDescription.Type, use);
		}
	}

	std::unordered_map<std::uint32_t, ImageState> states;
	states.reserve(ImageBindings.size());
	std::unordered_map<std::uint32_t, BufferState> bufferStates;
	bufferStates.reserve(BufferBindings.size());

	for (const auto pass : compilation.ExecutionOrder)
	{
		const auto& passDescription = Graph.GetPassDescription(pass);
		for (const auto& use : passDescription.Uses)
		{
			const auto bindingIterator = ImageBindings.find(use.Resource.Index);
			const auto& binding = bindingIterator->second;
			const auto usage = TranslateUsage(
				passDescription.Type,
				use,
				binding.AspectFlags);
			const auto& description = Graph.GetImageDescription(use.Resource);

			auto stateIterator = states.find(use.Resource.Index);
			if (stateIterator == states.end())
			{
				ImageState initialState{
					.Resource = use.Resource,
					.StageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
					.AccessMask = {},
					.Layout = description.InitialLayout,
					.HasBeenUsed = false,
					.LastUseWrites = false};
				stateIterator = states.emplace(use.Resource.Index, initialState).first;
			}

			auto& state = stateIterator->second;
			const bool stateChanged =
				state.Layout != usage.Layout ||
				state.StageMask != usage.StageMask ||
				state.AccessMask != usage.AccessMask;
			const bool needsMemoryDependency =
				state.HasBeenUsed && (state.LastUseWrites || usage.Writes);
			const bool needsBarrier = !state.HasBeenUsed
				? state.Layout != usage.Layout
				: stateChanged || needsMemoryDependency;

			if (needsBarrier)
			{
				EmitImageBarrier(
					commandBuffer,
					description,
					binding,
					state.StageMask,
					state.AccessMask,
					usage.StageMask,
					usage.AccessMask,
					state.Layout,
					usage.Layout);
			}

			state.StageMask = usage.StageMask;
			state.AccessMask = usage.AccessMask;
			state.Layout = usage.Layout;
			state.HasBeenUsed = true;
			state.LastUseWrites = usage.Writes;
		}

		for (const auto& use : passDescription.BufferUses)
		{
			const auto bindingIterator = BufferBindings.find(use.Resource.Index);
			const auto& binding = bindingIterator->second;
			const auto usage = TranslateUsage(passDescription.Type, use);

			auto stateIterator = bufferStates.find(use.Resource.Index);
			if (stateIterator == bufferStates.end())
			{
				BufferState initialState{
					.Resource = use.Resource,
					.StageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
					.AccessMask = {},
					.HasBeenUsed = false,
					.LastUseWrites = false};
				stateIterator = bufferStates.emplace(use.Resource.Index, initialState).first;
			}

			auto& state = stateIterator->second;
			const bool stateChanged =
				state.StageMask != usage.StageMask ||
				state.AccessMask != usage.AccessMask;
			const bool needsMemoryDependency =
				state.HasBeenUsed && (state.LastUseWrites || usage.Writes);
			const bool needsBarrier = !state.HasBeenUsed
				? state.AccessMask != usage.AccessMask
				: stateChanged || needsMemoryDependency;

			if (needsBarrier)
			{
				EmitBufferBarrier(
					commandBuffer,
					binding,
					state.StageMask,
					state.AccessMask,
					usage.StageMask,
					usage.AccessMask);
			}

			state.StageMask = usage.StageMask;
			state.AccessMask = usage.AccessMask;
			state.HasBeenUsed = true;
			state.LastUseWrites = usage.Writes;
		}

		// The callback owns the dynamic-rendering scope. The executor only
		// guarantees that declared image states are ready before it is called.
		passCallback(commandBuffer, pass);
	}

	for (const auto& [resourceIndex, state] : states)
	{
		const auto bindingIterator = ImageBindings.find(resourceIndex);
		if (bindingIterator == ImageBindings.end())
		{
			continue;
		}

		const auto& binding = bindingIterator->second;
		const auto& description = Graph.GetImageDescription(state.Resource);
		if (state.Layout == description.FinalLayout)
		{
			continue;
		}

		EmitImageBarrier(
			commandBuffer,
			description,
			binding,
			state.StageMask,
			state.AccessMask,
			vk::PipelineStageFlagBits2::eBottomOfPipe,
			{},
			state.Layout,
			description.FinalLayout);
	}
}

RenderGraphExecutor::UsageState RenderGraphExecutor::TranslateUsage(
	RenderGraph::PassType passType,
	const RenderGraph::ImageUse& use,
	vk::ImageAspectFlags aspectFlags)
{
	return UsageState{
		.StageMask = TranslateStage(passType, use.Usage),
		.AccessMask = TranslateAccess(use.Usage, use.Access),
		.Layout = TranslateLayout(use.Usage, aspectFlags),
		.Writes = IncludesWrite(use.Access)};
}

RenderGraphExecutor::UsageState RenderGraphExecutor::TranslateUsage(
	RenderGraph::PassType passType,
	const RenderGraph::BufferUse& use)
{
	return UsageState{
		.StageMask = TranslateStage(passType, use.Usage),
		.AccessMask = TranslateAccess(use.Usage, use.Access),
		.Layout = vk::ImageLayout::eUndefined,
		.Writes = IncludesWrite(use.Access)};
}

vk::PipelineStageFlags2 RenderGraphExecutor::TranslateStage(
	RenderGraph::PassType passType,
	RenderGraph::ImageUsage usage)
{
	switch (usage)
	{
		case RenderGraph::ImageUsage::ColorAttachment:
			return vk::PipelineStageFlagBits2::eColorAttachmentOutput;
		case RenderGraph::ImageUsage::DepthAttachment:
			return vk::PipelineStageFlagBits2::eEarlyFragmentTests |
				vk::PipelineStageFlagBits2::eLateFragmentTests;
		case RenderGraph::ImageUsage::TransferSource:
		case RenderGraph::ImageUsage::TransferDestination:
			return vk::PipelineStageFlagBits2::eTransfer;
		case RenderGraph::ImageUsage::Sampled:
		case RenderGraph::ImageUsage::Storage:
			switch (passType)
			{
				case RenderGraph::PassType::Graphics:
					return vk::PipelineStageFlagBits2::eAllGraphics;
				case RenderGraph::PassType::Compute:
					return vk::PipelineStageFlagBits2::eComputeShader;
				case RenderGraph::PassType::Transfer:
					throw std::invalid_argument(
						"Sampled or storage image use is invalid in a transfer pass");
			}
			break;
	}

	throw std::invalid_argument("Unknown RenderGraph image usage");
}

vk::PipelineStageFlags2 RenderGraphExecutor::TranslateStage(
	RenderGraph::PassType passType,
	RenderGraph::BufferUsage usage)
{
	switch (usage)
	{
		case RenderGraph::BufferUsage::Storage:
			return passType == RenderGraph::PassType::Compute
				? vk::PipelineStageFlagBits2::eComputeShader
				: vk::PipelineStageFlagBits2::eAllGraphics;
		case RenderGraph::BufferUsage::Vertex:
			return vk::PipelineStageFlagBits2::eVertexInput;
		case RenderGraph::BufferUsage::Index:
			return vk::PipelineStageFlagBits2::eVertexInput;
		case RenderGraph::BufferUsage::Uniform:
			return passType == RenderGraph::PassType::Compute
				? vk::PipelineStageFlagBits2::eComputeShader
				: vk::PipelineStageFlagBits2::eAllGraphics;
		case RenderGraph::BufferUsage::TransferSource:
		case RenderGraph::BufferUsage::TransferDestination:
			return vk::PipelineStageFlagBits2::eTransfer;
	}

	throw std::invalid_argument("Unknown RenderGraph buffer usage");
}

vk::AccessFlags2 RenderGraphExecutor::TranslateAccess(
	RenderGraph::ImageUsage usage,
	RenderGraph::AccessType access)
{
	const bool reads = IncludesRead(access);
	const bool writes = IncludesWrite(access);
	vk::AccessFlags2 result{};

	switch (usage)
	{
		case RenderGraph::ImageUsage::Sampled:
			if (writes)
			{
				throw std::invalid_argument(
					"Sampled image usage cannot declare a write access");
			}
			if (reads)
			{
				result |= vk::AccessFlagBits2::eShaderSampledRead;
			}
			return result;
		case RenderGraph::ImageUsage::Storage:
			if (reads)
			{
				result |= vk::AccessFlagBits2::eShaderStorageRead;
			}
			if (writes)
			{
				result |= vk::AccessFlagBits2::eShaderStorageWrite;
			}
			return result;
		case RenderGraph::ImageUsage::ColorAttachment:
			if (reads)
			{
				result |= vk::AccessFlagBits2::eColorAttachmentRead;
			}
			if (writes)
			{
				result |= vk::AccessFlagBits2::eColorAttachmentWrite;
			}
			return result;
		case RenderGraph::ImageUsage::DepthAttachment:
			if (reads)
			{
				result |= vk::AccessFlagBits2::eDepthStencilAttachmentRead;
			}
			if (writes)
			{
				result |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
			}
			return result;
		case RenderGraph::ImageUsage::TransferSource:
			if (writes)
			{
				throw std::invalid_argument(
					"Transfer source image usage cannot declare a write access");
			}
			return reads ? vk::AccessFlagBits2::eTransferRead : vk::AccessFlags2{};
		case RenderGraph::ImageUsage::TransferDestination:
			if (reads)
			{
				result |= vk::AccessFlagBits2::eTransferRead;
			}
			if (writes)
			{
				result |= vk::AccessFlagBits2::eTransferWrite;
			}
			return result;
	}

	throw std::invalid_argument("Unknown RenderGraph image usage");
}

vk::AccessFlags2 RenderGraphExecutor::TranslateAccess(
	RenderGraph::BufferUsage usage,
	RenderGraph::AccessType access)
{
	const bool reads = IncludesRead(access);
	const bool writes = IncludesWrite(access);
	vk::AccessFlags2 result{};

	switch (usage)
	{
		case RenderGraph::BufferUsage::Storage:
			if (reads)
			{
				result |= vk::AccessFlagBits2::eShaderStorageRead;
			}
			if (writes)
			{
				result |= vk::AccessFlagBits2::eShaderStorageWrite;
			}
			return result;
		case RenderGraph::BufferUsage::Vertex:
			if (writes)
			{
				throw std::invalid_argument("Vertex buffer usage cannot declare a write access");
			}
			return reads ? vk::AccessFlagBits2::eVertexAttributeRead : vk::AccessFlags2{};
		case RenderGraph::BufferUsage::Index:
			if (writes)
			{
				throw std::invalid_argument("Index buffer usage cannot declare a write access");
			}
			return reads ? vk::AccessFlagBits2::eIndexRead : vk::AccessFlags2{};
		case RenderGraph::BufferUsage::Uniform:
			if (writes)
			{
				throw std::invalid_argument("Uniform buffer usage cannot declare a write access");
			}
			return reads ? vk::AccessFlagBits2::eUniformRead : vk::AccessFlags2{};
		case RenderGraph::BufferUsage::TransferSource:
			if (writes)
			{
				throw std::invalid_argument("Transfer source buffer usage cannot declare a write access");
			}
			return reads ? vk::AccessFlagBits2::eTransferRead : vk::AccessFlags2{};
		case RenderGraph::BufferUsage::TransferDestination:
			if (reads)
			{
				result |= vk::AccessFlagBits2::eTransferRead;
			}
			if (writes)
			{
				result |= vk::AccessFlagBits2::eTransferWrite;
			}
			return result;
	}

	throw std::invalid_argument("Unknown RenderGraph buffer usage");
}

vk::ImageLayout RenderGraphExecutor::TranslateLayout(
	RenderGraph::ImageUsage usage,
	vk::ImageAspectFlags aspectFlags)
{
	switch (usage)
	{
		case RenderGraph::ImageUsage::Sampled:
			return vk::ImageLayout::eShaderReadOnlyOptimal;
		case RenderGraph::ImageUsage::Storage:
			return vk::ImageLayout::eGeneral;
		case RenderGraph::ImageUsage::ColorAttachment:
			return vk::ImageLayout::eColorAttachmentOptimal;
		case RenderGraph::ImageUsage::DepthAttachment:
			return aspectFlags & vk::ImageAspectFlagBits::eStencil
				? vk::ImageLayout::eDepthStencilAttachmentOptimal
				: vk::ImageLayout::eDepthAttachmentOptimal;
		case RenderGraph::ImageUsage::TransferSource:
			return vk::ImageLayout::eTransferSrcOptimal;
		case RenderGraph::ImageUsage::TransferDestination:
			return vk::ImageLayout::eTransferDstOptimal;
	}

	throw std::invalid_argument("Unknown RenderGraph image usage");
}

bool RenderGraphExecutor::IncludesRead(RenderGraph::AccessType access) noexcept
{
	return access == RenderGraph::AccessType::Read ||
		access == RenderGraph::AccessType::ReadWrite;
}

bool RenderGraphExecutor::IncludesWrite(RenderGraph::AccessType access) noexcept
{
	return access == RenderGraph::AccessType::Write ||
		access == RenderGraph::AccessType::ReadWrite;
}

void RenderGraphExecutor::EmitImageBarrier(
	vk::raii::CommandBuffer& commandBuffer,
	const RenderGraph::ImageDescription& description,
	const ImageBinding& binding,
	vk::PipelineStageFlags2 sourceStage,
	vk::AccessFlags2 sourceAccess,
	vk::PipelineStageFlags2 destinationStage,
	vk::AccessFlags2 destinationAccess,
	vk::ImageLayout oldLayout,
	vk::ImageLayout newLayout) const
{
	if (oldLayout == newLayout && sourceStage == destinationStage &&
		sourceAccess == destinationAccess)
	{
		return;
	}

	const vk::ImageMemoryBarrier2 barrier{
		.srcStageMask = sourceStage,
		.srcAccessMask = sourceAccess,
		.dstStageMask = destinationStage,
		.dstAccessMask = destinationAccess,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = binding.Image,
		.subresourceRange = {
			.aspectMask = binding.AspectFlags,
			.baseMipLevel = 0,
			.levelCount = description.MipLevels,
			.baseArrayLayer = 0,
			.layerCount = description.Layers}};
	const vk::DependencyInfo dependencyInfo{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier};
	commandBuffer.pipelineBarrier2(dependencyInfo);
}

void RenderGraphExecutor::EmitBufferBarrier(
	vk::raii::CommandBuffer& commandBuffer,
	const BufferBinding& binding,
	vk::PipelineStageFlags2 sourceStage,
	vk::AccessFlags2 sourceAccess,
	vk::PipelineStageFlags2 destinationStage,
	vk::AccessFlags2 destinationAccess) const
{
	if (sourceStage == destinationStage && sourceAccess == destinationAccess)
	{
		return;
	}

	const vk::BufferMemoryBarrier2 barrier{
		.srcStageMask = sourceStage,
		.srcAccessMask = sourceAccess,
		.dstStageMask = destinationStage,
		.dstAccessMask = destinationAccess,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.buffer = binding.Buffer,
		.offset = 0,
		.size = vk::WholeSize};
	const vk::DependencyInfo dependencyInfo{
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers = &barrier};
	commandBuffer.pipelineBarrier2(dependencyInfo);
}
