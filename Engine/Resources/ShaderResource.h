#pragma once

#include "Resource.h"
#include "../Vulkan/VulkanContext.h"

class ShaderResource final : public Resource
{
  public:
	ShaderResource(std::string resourceId, VulkanContext& vulkan, std::string filePath);

	ShaderResource(const ShaderResource&) = delete;
	ShaderResource& operator=(const ShaderResource&) = delete;
	ShaderResource(ShaderResource&&) = delete;
	ShaderResource& operator=(ShaderResource&&) = delete;

	[[nodiscard]] const vk::raii::ShaderModule& module() const noexcept;
	[[nodiscard]] const std::string& filePath() const noexcept;

  protected:
	[[nodiscard]] bool DoLoad() override;
	void DoUnload() override;

  private:
	VulkanContext& Vulkan;
	std::string FilePath;
	vk::raii::ShaderModule Module = nullptr;
};
