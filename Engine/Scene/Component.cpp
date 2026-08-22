#include "Component.h"

#include <cassert>

Component::~Component() = default;

Component::State Component::state() const noexcept
{
	return state_;
}

bool Component::isActive() const noexcept
{
	return state_ == State::Active;
}

GameObject& Component::owner() noexcept
{
	assert(owner_ != nullptr);
	return *owner_;
}

const GameObject& Component::owner() const noexcept
{
	assert(owner_ != nullptr);
	return *owner_;
}

void Component::onInitialize()
{
}

void Component::onUpdate(float)
{
}

void Component::onDestroy() noexcept
{
}

void Component::attach(GameObject& owner) noexcept
{
	assert(owner_ == nullptr);
	owner_ = &owner;
}

void Component::initialize()
{
	if (state_ != State::Uninitialized)
	{
		return;
	}

	state_ = State::Initializing;
	try
	{
		onInitialize();
		state_ = State::Active;
	}
	catch (...)
	{
		state_ = State::Destroying;
		onDestroy();
		state_ = State::Destroyed;
		throw;
	}
}

void Component::update(float deltaTime)
{
	if (state_ == State::Active)
	{
		onUpdate(deltaTime);
	}
}

void Component::destroy() noexcept
{
	if (state_ == State::Destroyed || state_ == State::Destroying)
	{
		return;
	}

	if (state_ == State::Active)
	{
		state_ = State::Destroying;
		onDestroy();
	}
	state_ = State::Destroyed;
}
