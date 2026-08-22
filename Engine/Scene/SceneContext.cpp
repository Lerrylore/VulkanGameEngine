#include "SceneContext.h"

#include "../Events/EventBus.h"
#include "../Services/ServiceLocator.h"

SceneContext::SceneContext(ServiceLocator& serviceLocator) noexcept : Services(serviceLocator)
{
}

EventBus& SceneContext::GetEventBus()
{
	return Services.Get<EventBus>();
}

const EventBus& SceneContext::GetEventBus() const
{
	return Services.Get<EventBus>();
}
