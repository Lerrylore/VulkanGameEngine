#include "TransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>

void TransformComponent::SetPosition(const glm::vec3& position) noexcept
{
	Position = position;
}

void TransformComponent::SetRotation(const glm::vec3& rotation) noexcept
{
	Rotation = rotation;
}

void TransformComponent::SetScale(const glm::vec3& scale) noexcept
{
	Scale = scale;
}

const glm::vec3& TransformComponent::GetPosition() const noexcept
{
	return Position;
}

const glm::vec3& TransformComponent::GetRotation() const noexcept
{
	return Rotation;
}

const glm::vec3& TransformComponent::GetScale() const noexcept
{
	return Scale;
}

glm::mat4 TransformComponent::ModelMatrix() const
{
	glm::mat4 model{1.0f};
	model = glm::translate(model, Position);
	model = glm::rotate(model, Rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, Rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
	return glm::scale(model, Scale);
}
