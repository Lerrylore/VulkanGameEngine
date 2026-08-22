#include "GameObject.h"
#include "SceneContext.h"
#include "TransformComponent.h"

#include <algorithm>

GameObject::GameObject() : Transform(&AddComponent<TransformComponent>())
{
}

GameObject::~GameObject()
{
	Destroy();
}

TransformComponent& GameObject::GetTransform() noexcept
{
	return *Transform;
}

const TransformComponent& GameObject::GetTransform() const noexcept
{
	return *Transform;
}

bool GameObject::IsInitialized() const noexcept
{
	return bInitialized;
}

bool GameObject::RemoveComponent(Component& component)
{
	if (bDestroyed)
	{
		throw std::logic_error("Cannot remove a component from a destroyed GameObject");
	}
	if (&component == Transform)
	{
		throw std::logic_error("TransformComponent cannot be removed from a GameObject");
	}
	if (IsPendingRemoval(component))
	{
		return false;
	}

	const auto componentIterator = std::find_if(
		Components.begin(), Components.end(), [&component](const std::unique_ptr<Component>& candidate)
		{
			return candidate.get() == &component;
		});
	if (componentIterator == Components.end())
	{
		return false;
	}

	PendingComponentRemovals.push_back(&component);
	if (!bUpdating)
	{
		RemovePendingComponents();
	}
	return true;
}

void GameObject::Initialize(SceneContext& context)
{
	if (bDestroyed)
	{
		throw std::logic_error("Cannot initialize a destroyed GameObject");
	}
	if (bInitialized)
	{
		return;
	}

	Context = &context;
	try
	{
		for (size_t index = 0; index < Components.size(); ++index)
		{
			Components[index]->Initialize(context);
		}
		bInitialized = true;
	}
	catch (...)
	{
		Destroy();
		throw;
	}
}

void GameObject::Update(float deltaTime)
{
	if (!bInitialized || bDestroyed)
	{
		return;
	}

	bUpdating = true;
	try
	{
		const size_t componentCount = Components.size();
		for (size_t index = 0; index < componentCount; ++index)
		{
			if (!IsPendingRemoval(*Components[index]))
			{
				Components[index]->Update(deltaTime);
			}
			if (bDestroyed)
			{
				break;
			}
		}
	}
	catch (...)
	{
		bUpdating = false;
		if (bDestroyed)
		{
			for (auto component = Components.rbegin(); component != Components.rend(); ++component)
			{
				(*component)->Destroy();
			}
			PendingComponentRemovals.clear();
			Context = nullptr;
		}
		else
		{
			RemovePendingComponents();
		}
		throw;
	}

	bUpdating = false;
	if (bDestroyed)
	{
		for (auto component = Components.rbegin(); component != Components.rend(); ++component)
		{
			(*component)->Destroy();
		}
		PendingComponentRemovals.clear();
		Context = nullptr;
	}
	else
	{
		RemovePendingComponents();
	}
}

void GameObject::Destroy() noexcept
{
	if (bDestroyed)
	{
		return;
	}

	bDestroyed = true;
	bInitialized = false;
	if (bUpdating)
	{
		return;
	}

	for (auto component = Components.rbegin(); component != Components.rend(); ++component)
	{
		(*component)->Destroy();
	}
	PendingComponentRemovals.clear();
	Context = nullptr;
}

bool GameObject::IsPendingRemoval(const Component& component) const noexcept
{
	return std::find(PendingComponentRemovals.begin(), PendingComponentRemovals.end(), &component) !=
		PendingComponentRemovals.end();
}

void GameObject::RemovePendingComponents() noexcept
{
	if (PendingComponentRemovals.empty())
	{
		return;
	}

	bUpdating = true;
	while (true)
	{
		Component* componentToDestroy = nullptr;
		for (size_t index = Components.size(); index > 0; --index)
		{
			Component& component = *Components[index - 1];
			if (IsPendingRemoval(component) && component.GetState() != Component::State::Destroyed &&
				component.GetState() != Component::State::Destroying)
			{
				componentToDestroy = &component;
				break;
			}
		}
		if (componentToDestroy == nullptr)
		{
			break;
		}
		componentToDestroy->Destroy();
		if (bDestroyed)
		{
			break;
		}
	}

	if (bDestroyed)
	{
		for (auto component = Components.rbegin(); component != Components.rend(); ++component)
		{
			if ((*component)->GetState() != Component::State::Destroyed &&
				(*component)->GetState() != Component::State::Destroying)
			{
				(*component)->Destroy();
			}
		}
		PendingComponentRemovals.clear();
		bUpdating = false;
		Context = nullptr;
		return;
	}

	Components.erase(
		std::remove_if(Components.begin(), Components.end(), [this](const std::unique_ptr<Component>& component)
		{
			return IsPendingRemoval(*component);
		}),
		Components.end());
	PendingComponentRemovals.clear();
	bUpdating = false;
}
