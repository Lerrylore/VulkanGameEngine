#pragma once

#include "../Resources/ImageAllocation.h"
#include "SwapchainResources.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

// Single-sample, shader-readable attachments used by the deferred path.
// Each frame in flight owns a complete set so recording/submission cannot
// overwrite a G-buffer still consumed by an older GPU frame.
class GBufferResources final
{
  public:
	static constexpr std::size_t AttachmentCount = 4;

	GBufferResources(VulkanContext& vulkan, const SwapchainResources& swapchain, uint32_t framesInFlight);

	GBufferResources(const GBufferResources&) = delete;
	GBufferResources& operator=(const GBufferResources&) = delete;
	GBufferResources(GBufferResources&&) = delete;
	GBufferResources& operator=(GBufferResources&&) = delete;

	void Recreate();

	[[nodiscard]] const vk::raii::Image& ColorImage(uint32_t frameIndex, std::size_t attachment) const;
	[[nodiscard]] const vk::raii::ImageView& ColorView(uint32_t frameIndex, std::size_t attachment) const;
	[[nodiscard]] const vk::raii::Image& DepthImage(uint32_t frameIndex) const;
	[[nodiscard]] const vk::raii::ImageView& DepthView(uint32_t frameIndex) const;
	[[nodiscard]] vk::Format ColorFormat(std::size_t attachment) const noexcept;
	[[nodiscard]] vk::Format DepthFormat() const noexcept;
	[[nodiscard]] uint32_t FrameCount() const noexcept;

  private:
	struct Frame final
	{
		std::array<std::unique_ptr<ImageAllocation>, AttachmentCount> Colors;
		std::vector<vk::raii::ImageView> ColorViews;
		std::unique_ptr<ImageAllocation> Depth;
		vk::raii::ImageView DepthView = nullptr;
	};

	void CreateFrame(Frame& frame);
	[[nodiscard]] vk::raii::ImageView CreateView(
		const vk::raii::Image& image,
		vk::Format format,
		vk::ImageAspectFlags aspect) const;
	[[nodiscard]] vk::Format FindDepthFormat() const;
	void ValidateFrameIndex(uint32_t frameIndex) const;
	void ValidateAttachmentIndex(std::size_t attachment) const;

	VulkanContext& Vulkan;
	const SwapchainResources& Swapchain;
	const uint32_t FramesInFlight;
	const std::array<vk::Format, AttachmentCount> Formats{
		vk::Format::eR8G8B8A8Unorm,
		vk::Format::eR16G16B16A16Sfloat,
		vk::Format::eR16G16B16A16Sfloat,
		vk::Format::eR16G16B16A16Sfloat};
	vk::Format SelectedDepthFormat = vk::Format::eUndefined;
	std::vector<std::unique_ptr<Frame>> Frames;
};
