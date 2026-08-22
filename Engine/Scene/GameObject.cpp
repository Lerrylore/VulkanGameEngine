#include "GameObject.h"
#include "TransformComponent.h"

GameObject::GameObject() : transform_(&addComponent<TransformComponent>())
{
}

GameObject::~GameObject()
{
	destroy();
}

TransformComponent& GameObject::transform() noexcept
{
	return *transform_;
}

const TransformComponent& GameObject::transform() const noexcept
{
	return *transform_;
}

bool GameObject::isInitialized() const noexcept
{
	return initialized_;
}

void GameObject::initialize()
{
	if (destroyed_)
	{
		throw std::logic_error("Cannot initialize a destroyed GameObject");
	}
	if (initialized_)
	{
		return;
	}

	try
	{
		for (size_t index = 0; index < components_.size(); ++index)
		{
			components_[index]->initialize();
		}
		initialized_ = true;
	}
	catch (...)
	{
		destroy();
		throw;
	}
}

void GameObject::update(float deltaTime)
{
	if (!initialized_ || destroyed_)
	{
		return;
	}

	const size_t componentCount = components_.size();
	for (size_t index = 0; index < componentCount; ++index)
	{
		components_[index]->update(deltaTime);
	}
}

void GameObject::destroy() noexcept
{
	if (destroyed_)
	{
		return;
	}

	destroyed_ = true;
	initialized_ = false;
	for (auto component = components_.rbegin(); component != components_.rend(); ++component)
	{
		(*component)->destroy();
	}
}
