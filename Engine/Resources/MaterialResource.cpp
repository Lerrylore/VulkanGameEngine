#include "MaterialResource.h"

#include "TextureResource.h"

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
