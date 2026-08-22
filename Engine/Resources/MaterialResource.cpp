#include "MaterialResource.h"

#include "TextureResource.h"

#include <stdexcept>
#include <utility>

MaterialResource::MaterialResource(
	std::string resourceId,
	TextureResource& baseColorTexture,
	TextureResource& normalTexture,
	TextureResource& metallicRoughnessTexture) noexcept
	: Resource(std::move(resourceId)),
	  BaseColorTexture(baseColorTexture),
	  NormalTexture(normalTexture),
	  MetallicRoughnessTexture(metallicRoughnessTexture)
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

TextureResource& MaterialResource::GetNormalTexture() noexcept
{
	return NormalTexture;
}

const TextureResource& MaterialResource::GetNormalTexture() const noexcept
{
	return NormalTexture;
}

TextureResource& MaterialResource::GetMetallicRoughnessTexture() noexcept
{
	return MetallicRoughnessTexture;
}

const TextureResource& MaterialResource::GetMetallicRoughnessTexture() const noexcept
{
	return MetallicRoughnessTexture;
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

float MaterialResource::GetOcclusionStrength() const noexcept
{
	return OcclusionStrength;
}

void MaterialResource::SetOcclusionStrength(float strength)
{
	if (strength < 0.0f || strength > 1.0f)
	{
		throw std::invalid_argument("Material occlusion strength must be between 0 and 1");
	}
	OcclusionStrength = strength;
}

const glm::vec4& MaterialResource::GetEmissive() const noexcept
{
	return Emissive;
}

void MaterialResource::SetEmissive(const glm::vec4& emissive) noexcept
{
	Emissive = emissive;
}

bool MaterialResource::DoLoad()
{
	return true;
}

void MaterialResource::DoUnload()
{
}
