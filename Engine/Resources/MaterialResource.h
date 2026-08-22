#pragma once

class TextureResource;

class MaterialResource final
{
  public:
	explicit MaterialResource(TextureResource& baseColorTexture) noexcept;

	MaterialResource(const MaterialResource&) = delete;
	MaterialResource& operator=(const MaterialResource&) = delete;
	MaterialResource(MaterialResource&&) = delete;
	MaterialResource& operator=(MaterialResource&&) = delete;

	[[nodiscard]] TextureResource& GetBaseColorTexture() noexcept;
	[[nodiscard]] const TextureResource& GetBaseColorTexture() const noexcept;

  private:
	// The texture owner must outlive every material that references it.
	TextureResource& BaseColorTexture;
};
