#pragma once

#include <glm/glm.hpp>

class GameObject final
{
  public:
	glm::vec3 position{0.0f};
	glm::vec3 rotation{0.0f};
	glm::vec3 scale{1.0f};

	[[nodiscard]] glm::mat4 modelMatrix() const;
};
