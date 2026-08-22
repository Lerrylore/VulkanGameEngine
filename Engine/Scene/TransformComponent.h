#pragma once

#include "Component.h"

#include <glm/glm.hpp>

class TransformComponent final : public Component
{
  public:
	void SetPosition(const glm::vec3& position) noexcept;
	void SetRotation(const glm::vec3& rotation) noexcept;
	void SetScale(const glm::vec3& scale) noexcept;

	[[nodiscard]] const glm::vec3& GetPosition() const noexcept;
	[[nodiscard]] const glm::vec3& GetRotation() const noexcept;
	[[nodiscard]] const glm::vec3& GetScale() const noexcept;
	[[nodiscard]] glm::mat4 ModelMatrix() const;

  private:
	glm::vec3 Position{0.0f};
	glm::vec3 Rotation{0.0f};
	glm::vec3 Scale{1.0f};
};
