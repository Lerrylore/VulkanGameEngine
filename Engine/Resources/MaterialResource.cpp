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

const glm::vec3& MaterialResource::GetSpecularColor() const noexcept
{
	return SpecularColor;
}

void MaterialResource::SetSpecularColor(const glm::vec3& specularColor) noexcept
{
	SpecularColor = specularColor;
}

float MaterialResource::GetShininess() const noexcept
{
	return Shininess;
}

void MaterialResource::SetShininess(float shininess)
{
	if (shininess <= 0.0f)
	{
		throw std::invalid_argument("Material shininess must be positive");
	}
	Shininess = shininess;
}
