#pragma once

#include "SwapchainResources.h"

class RenderTargetResources final
{
  public:
	RenderTargetResources(VulkanContext& vulkan, const SwapchainResources& swapchain);

	RenderTargetResources(const RenderTargetResources&) = delete;
	RenderTargetResources& operator=(const RenderTargetResources&) = delete;
	RenderTargetResources(RenderTargetResources&&) = delete;
	RenderTargetResources& operator=(RenderTargetResources&&) = delete;

	void recreate();

	[[nodiscard]] vk::raii::Image& colorImage() noexcept;
	[[nodiscard]] vk::raii::ImageView& colorImageView() noexcept;
	[[nodiscard]] vk::raii::Image& depthImage() noexcept;
	[[nodiscard]] vk::raii::ImageView& depthImageView() noexcept;
	[[nodiscard]] vk::Format depthFormat() const noexcept;

  private:
	struct AttachmentImage
	{
		// Reverse destruction order: view, image, then the bound memory.
		vk::raii::DeviceMemory memory = nullptr;
		vk::raii::Image image = nullptr;
		vk::raii::ImageView view = nullptr;

		void reset();
	};

	void createColorAttachment();
	void createDepthAttachment();
	void createAttachmentImage(
		AttachmentImage& attachment,
		vk::Format format,
		vk::ImageUsageFlags usage);
	[[nodiscard]] vk::raii::ImageView createImageView(
		const vk::raii::Image& image,
		vk::Format format,
		vk::ImageAspectFlags aspectFlags) const;
	[[nodiscard]] vk::Format findDepthFormat() const;
	[[nodiscard]] uint32_t findMemoryType(
		uint32_t typeFilter,
		vk::MemoryPropertyFlags properties) const;

	VulkanContext& vulkan;
	const SwapchainResources& swapchain;
	AttachmentImage colorAttachment;
	AttachmentImage depthAttachment;
	vk::Format selectedDepthFormat = vk::Format::eUndefined;
};
