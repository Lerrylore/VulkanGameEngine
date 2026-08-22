#include "TransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>

void TransformComponent::setPosition(const glm::vec3& position) noexcept
{
	position_ = position;
}

void TransformComponent::setRotation(const glm::vec3& rotation) noexcept
{
	rotation_ = rotation;
}

void TransformComponent::setScale(const glm::vec3& scale) noexcept
{
	scale_ = scale;
}

const glm::vec3& TransformComponent::position() const noexcept
{
	return position_;
}

const glm::vec3& TransformComponent::rotation() const noexcept
{
	return rotation_;
}

const glm::vec3& TransformComponent::scale() const noexcept
{
	return scale_;
}

glm::mat4 TransformComponent::modelMatrix() const
{
	glm::mat4 model{1.0f};
	model = glm::translate(model, position_);
	model = glm::rotate(model, rotation_.x, glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, rotation_.y, glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, rotation_.z, glm::vec3(0.0f, 0.0f, 1.0f));
	return glm::scale(model, scale_);
}
