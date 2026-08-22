#include "GameObject.h"
#include "TransformComponent.h"

GameObject::GameObject() : transform_(&addComponent<TransformComponent>())
{
}

GameObject::~GameObject() = default;

TransformComponent& GameObject::transform() noexcept
{
	return *transform_;
}

const TransformComponent& GameObject::transform() const noexcept
{
	return *transform_;
}
