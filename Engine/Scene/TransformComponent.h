#pragma once

#include "Component.h"

#include <glm/glm.hpp>

class TransformComponent final : public Component
{
  public:
	void setPosition(const glm::vec3& position) noexcept;
	void setRotation(const glm::vec3& rotation) noexcept;
	void setScale(const glm::vec3& scale) noexcept;

	[[nodiscard]] const glm::vec3& position() const noexcept;
	[[nodiscard]] const glm::vec3& rotation() const noexcept;
	[[nodiscard]] const glm::vec3& scale() const noexcept;
	[[nodiscard]] glm::mat4 modelMatrix() const;

  private:
	glm::vec3 position_{0.0f};
	glm::vec3 rotation_{0.0f};
	glm::vec3 scale_{1.0f};
};
