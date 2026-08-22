#include "Component.h"

#include "SceneContext.h"

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

EventBus& Component::GetEventBus()
{
	assert(Context != nullptr);
	return Context->GetEventBus();
}

const EventBus& Component::GetEventBus() const
{
	assert(Context != nullptr);
	return Context->GetEventBus();
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

void Component::Initialize(SceneContext& context)
{
	if (CurrentState != State::Uninitialized)
	{
		return;
	}

	Context = &context;
	CurrentState = State::Initializing;
	try
	{
		OnInitialize();
		if (CurrentState == State::Initializing)
		{
			CurrentState = State::Active;
		}
	}
	catch (...)
	{
		if (CurrentState != State::Destroyed)
		{
			CurrentState = State::Destroying;
			ResetEventSubscriptions();
			OnDestroy();
			Context = nullptr;
			CurrentState = State::Destroyed;
		}
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

	if (CurrentState == State::Active || CurrentState == State::Initializing)
	{
		CurrentState = State::Destroying;
		ResetEventSubscriptions();
		OnDestroy();
	}
	Context = nullptr;
	CurrentState = State::Destroyed;
}

void Component::ResetEventSubscriptions() noexcept
{
	EventSubscriptions.clear();
}
