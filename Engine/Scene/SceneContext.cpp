#include "SceneContext.h"

#include "Events/EventDispatcher.h"

SceneContext::SceneContext(EventDispatcher& eventDispatcher) noexcept : Events(eventDispatcher)
{
}

EventDispatcher& SceneContext::GetEventDispatcher() noexcept
{
	return Events;
}

const EventDispatcher& SceneContext::GetEventDispatcher() const noexcept
{
	return Events;
}
