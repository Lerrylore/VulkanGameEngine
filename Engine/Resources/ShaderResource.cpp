#include "ShaderResource.h"

#include "BinaryFileLoader.h"

#include <exception>
#include <utility>

ShaderResource::ShaderResource(
	std::string resourceId,
	VulkanContext& vulkan,
	std::string filePath)
	: Resource(std::move(resourceId)), Vulkan(vulkan), FilePath(std::move(filePath))
{
}

const vk::raii::ShaderModule& ShaderResource::module() const noexcept
{
	return Module;
}

const std::string& ShaderResource::filePath() const noexcept
{
	return FilePath;
}

bool ShaderResource::DoLoad()
{
	try
	{
		const std::vector<char> code = BinaryFileLoader::Load(FilePath);
		const vk::ShaderModuleCreateInfo createInfo{
			.codeSize = code.size() * sizeof(char),
			.pCode = reinterpret_cast<const uint32_t*>(code.data())};
		Module = vk::raii::ShaderModule(Vulkan.device(), createInfo);
		return true;
	}
	catch (const std::exception&)
	{
		Module = nullptr;
		return false;
	}
}

void ShaderResource::DoUnload()
{
	Module = nullptr;
}
