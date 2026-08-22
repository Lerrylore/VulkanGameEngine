#include "MaterialResource.h"

#include "TextureResource.h"

#include <stdexcept>

MaterialResource::MaterialResource(TextureResource& baseColorTexture) noexcept
	: BaseColorTexture(baseColorTexture)
{
}

TextureResource& MaterialResource::GetBaseColorTexture() noexcept
{
	return BaseColorTexture;
}

const TextureResource& MaterialResource::GetBaseColorTexture() const noexcept
{
	return BaseColorTexture;
}

const glm::vec4& MaterialResource::GetBaseColor() const noexcept
{
	return BaseColor;
}

void MaterialResource::SetBaseColor(const glm::vec4& baseColor) noexcept
{
	BaseColor = baseColor;
}

float MaterialResource::GetMetallic() const noexcept
{
	return Metallic;
}

void MaterialResource::SetMetallic(float metallic)
{
	if (metallic < 0.0f || metallic > 1.0f)
	{
		throw std::invalid_argument("Material metallic value must be between 0 and 1");
	}
	Metallic = metallic;
}

float MaterialResource::GetRoughness() const noexcept
{
	return Roughness;
}

void MaterialResource::SetRoughness(float roughness)
{
	if (roughness <= 0.0f || roughness > 1.0f)
	{
		throw std::invalid_argument("Material roughness value must be greater than 0 and at most 1");
	}
	Roughness = roughness;
}
