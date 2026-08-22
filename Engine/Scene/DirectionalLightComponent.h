#pragma once

#include "Component.h"

#include <glm/glm.hpp>

class DirectionalLightComponent final : public Component
{
  public:
	void SetDirection(const glm::vec3& direction);
	void SetColor(const glm::vec3& color) noexcept;
	void SetIntensity(float intensity);
	void SetAmbientStrength(float ambientStrength);

	[[nodiscard]] const glm::vec3& GetDirection() const noexcept;
	[[nodiscard]] const glm::vec3& GetColor() const noexcept;
	[[nodiscard]] float GetIntensity() const noexcept;
	[[nodiscard]] float GetAmbientStrength() const noexcept;

  protected:
	void OnInitialize() override;
	void OnDestroy() noexcept override;

  private:
	// Direction is the world-space direction in which the light travels.
	glm::vec3 Direction{0.35f, -0.45f, -0.8f};
	glm::vec3 Color{1.0f};
	float Intensity = 1.0f;
	float AmbientStrength = 0.1f;
};
