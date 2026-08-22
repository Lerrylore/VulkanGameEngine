#include "EventSubscription.h"

#include "EventBus.h"

#include <utility>

EventSubscription::EventSubscription(EventBus& eventBus, const std::type_info& eventType,
	                                 uint64_t listenerId, std::weak_ptr<void> eventBusLifetime) noexcept
    : Bus(&eventBus), EventType(&eventType), ListenerId(listenerId),
      BusLifetime(std::move(eventBusLifetime))
{
}

EventSubscription::~EventSubscription()
{
	Reset();
}

EventSubscription::EventSubscription(EventSubscription&& other) noexcept
    : Bus(std::exchange(other.Bus, nullptr)), EventType(std::exchange(other.EventType, nullptr)),
	  ListenerId(std::exchange(other.ListenerId, 0)), BusLifetime(std::move(other.BusLifetime))
{
}

EventSubscription& EventSubscription::operator=(EventSubscription&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	Reset();
	Bus = std::exchange(other.Bus, nullptr);
	EventType = std::exchange(other.EventType, nullptr);
	ListenerId = std::exchange(other.ListenerId, 0);
	BusLifetime = std::move(other.BusLifetime);
	return *this;
}

void EventSubscription::Reset() noexcept
{
	if (Bus != nullptr && EventType != nullptr && !BusLifetime.expired())
	{
		Bus->Unsubscribe(*EventType, ListenerId);
	}

	Bus = nullptr;
	EventType = nullptr;
	ListenerId = 0;
	BusLifetime.reset();
}

bool EventSubscription::IsActive() const noexcept
{
	return Bus != nullptr && !BusLifetime.expired();
}
