#pragma once

#include <glm/glm.hpp>

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
	[[nodiscard]] const glm::vec4& GetBaseColor() const noexcept;
	void SetBaseColor(const glm::vec4& baseColor) noexcept;

  private:
	// The texture owner must outlive every material that references it.
	TextureResource& BaseColorTexture;
	glm::vec4 BaseColor{1.0f};
};
