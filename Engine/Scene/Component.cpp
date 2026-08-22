#include "Component.h"

#include <cassert>

Component::~Component() = default;

Component::State Component::GetState() const noexcept
{
	return CurrentState;
}

bool Component::IsActive() const noexcept
{
	return CurrentState == State::Active;
}

GameObject& Component::GetOwner() noexcept
{
	assert(Owner != nullptr);
	return *Owner;
}

const GameObject& Component::GetOwner() const noexcept
{
	assert(Owner != nullptr);
	return *Owner;
}

void Component::OnInitialize()
{
}

void Component::OnUpdate(float)
{
}

void Component::OnDestroy() noexcept
{
}

void Component::Attach(GameObject& owner) noexcept
{
	assert(Owner == nullptr);
	Owner = &owner;
}

void Component::Initialize()
{
	if (CurrentState != State::Uninitialized)
	{
		return;
	}

	CurrentState = State::Initializing;
	try
	{
		OnInitialize();
		CurrentState = State::Active;
	}
	catch (...)
	{
		CurrentState = State::Destroying;
		OnDestroy();
		CurrentState = State::Destroyed;
		throw;
	}
}

void Component::Update(float deltaTime)
{
	if (CurrentState == State::Active)
	{
		OnUpdate(deltaTime);
	}
}

void Component::Destroy() noexcept
{
	if (CurrentState == State::Destroyed || CurrentState == State::Destroying)
	{
		return;
	}

	if (CurrentState == State::Active)
	{
		CurrentState = State::Destroying;
		OnDestroy();
	}
	CurrentState = State::Destroyed;
}
