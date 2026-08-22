#pragma once

#include <glm/glm.hpp>

class TextureResource;

class MaterialResource final
{
  public:
	MaterialResource(
		TextureResource& baseColorTexture,
		TextureResource& normalTexture,
		TextureResource& metallicRoughnessTexture) noexcept;

	MaterialResource(const MaterialResource&) = delete;
	MaterialResource& operator=(const MaterialResource&) = delete;
	MaterialResource(MaterialResource&&) = delete;
	MaterialResource& operator=(MaterialResource&&) = delete;

	[[nodiscard]] TextureResource& GetBaseColorTexture() noexcept;
	[[nodiscard]] const TextureResource& GetBaseColorTexture() const noexcept;
	[[nodiscard]] TextureResource& GetNormalTexture() noexcept;
	[[nodiscard]] const TextureResource& GetNormalTexture() const noexcept;
	[[nodiscard]] TextureResource& GetMetallicRoughnessTexture() noexcept;
	[[nodiscard]] const TextureResource& GetMetallicRoughnessTexture() const noexcept;
	[[nodiscard]] const glm::vec4& GetBaseColor() const noexcept;
	void SetBaseColor(const glm::vec4& baseColor) noexcept;
	[[nodiscard]] float GetMetallic() const noexcept;
	void SetMetallic(float metallic);
	[[nodiscard]] float GetRoughness() const noexcept;
	void SetRoughness(float roughness);
	[[nodiscard]] float GetOcclusionStrength() const noexcept;
	void SetOcclusionStrength(float strength);
	[[nodiscard]] const glm::vec4& GetEmissive() const noexcept;
	void SetEmissive(const glm::vec4& emissive) noexcept;

  private:
	// The texture owner must outlive every material that references it.
	TextureResource& BaseColorTexture;
	TextureResource& NormalTexture;
	TextureResource& MetallicRoughnessTexture;
	glm::vec4 BaseColor{1.0f};
	float Metallic = 0.0f;
	float Roughness = 0.5f;
	float OcclusionStrength = 1.0f;
	glm::vec4 Emissive{0.0f};
};
