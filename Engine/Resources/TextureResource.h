#pragma once

#include "ImageAllocation.h"
#include "Resource.h"

#include <optional>

class TextureResource final : public Resource
{
  public:
	TextureResource(
		std::string resourceId,
		VulkanContext& vulkan,
		uint32_t width,
		uint32_t height,
		uint32_t mipLevels,
		vk::Format format);

	TextureResource(const TextureResource&) = delete;
	TextureResource& operator=(const TextureResource&) = delete;
	TextureResource(TextureResource&&) = delete;
	TextureResource& operator=(TextureResource&&) = delete;

	[[nodiscard]] vk::raii::Image& image() noexcept;
	[[nodiscard]] const vk::raii::Image& image() const noexcept;
	[[nodiscard]] const vk::raii::ImageView& imageView() const noexcept;
	[[nodiscard]] const vk::raii::Sampler& sampler() const noexcept;
	[[nodiscard]] uint32_t mipLevels() const noexcept;

	protected:
	[[nodiscard]] bool DoLoad() override;
	void DoUnload() override;

  private:
	// Reverse destruction releases sampler and view before their image.
	std::optional<ImageAllocation> imageAllocation;
	vk::raii::ImageView view = nullptr;
	vk::raii::Sampler samplerHandle = nullptr;
	uint32_t mipLevelCount = 0;
};
