#pragma once

#include "../Resources/ImageAllocation.h"

#include <cstdint>
#include <memory>
#include <vector>

class ShadowMapResources final
{
  public:
	ShadowMapResources(VulkanContext& vulkan, uint32_t framesInFlight, uint32_t resolution = 2048);

	ShadowMapResources(const ShadowMapResources&) = delete;
	ShadowMapResources& operator=(const ShadowMapResources&) = delete;
	ShadowMapResources(ShadowMapResources&&) = delete;
	ShadowMapResources& operator=(ShadowMapResources&&) = delete;

	[[nodiscard]] const vk::raii::Image& GetImage(uint32_t frameIndex) const noexcept;
	[[nodiscard]] const vk::raii::ImageView& GetImageView(uint32_t frameIndex) const noexcept;
	[[nodiscard]] const vk::raii::Sampler& GetSampler(uint32_t frameIndex) const noexcept;
	[[nodiscard]] vk::Format GetFormat() const noexcept;
	[[nodiscard]] uint32_t GetResolution() const noexcept;

  private:
	[[nodiscard]] static vk::Format FindDepthFormat(
		const vk::raii::PhysicalDevice& physicalDevice);
	void ValidateFrameIndex(uint32_t frameIndex) const;

	uint32_t FrameCount = 0;
	uint32_t Resolution = 0;
	vk::Format Format = vk::Format::eUndefined;
	std::vector<std::unique_ptr<ImageAllocation>> Images;
	std::vector<vk::raii::ImageView> ImageViews;
	std::vector<vk::raii::Sampler> Samplers;
};
