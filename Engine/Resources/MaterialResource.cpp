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
