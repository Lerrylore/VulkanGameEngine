#include "GameObject.h"
#include "TransformComponent.h"

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

void GameObject::Initialize()
{
	if (bDestroyed)
	{
		throw std::logic_error("Cannot initialize a destroyed GameObject");
	}
	if (bInitialized)
	{
		return;
	}

	try
	{
		for (size_t index = 0; index < Components.size(); ++index)
		{
			Components[index]->Initialize();
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

	const size_t componentCount = Components.size();
	for (size_t index = 0; index < componentCount; ++index)
	{
		Components[index]->Update(deltaTime);
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
	for (auto component = Components.rbegin(); component != Components.rend(); ++component)
	{
		(*component)->Destroy();
	}
}
