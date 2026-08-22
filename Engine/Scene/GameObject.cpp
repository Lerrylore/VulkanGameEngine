#include "GameObject.h"

#include <glm/gtc/matrix_transform.hpp>

glm::mat4 GameObject::modelMatrix() const
{
	glm::mat4 model{1.0f};
	model = glm::translate(model, position);
	model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
	return glm::scale(model, scale);
}
