#include "EventSubscription.h"

#include "EventDispatcher.h"

#include <utility>

EventSubscription::EventSubscription(EventDispatcher& dispatcher, const std::type_info& eventType,
                                     uint64_t listenerId, std::weak_ptr<void> dispatcherLifetime) noexcept
    : Dispatcher(&dispatcher), EventType(&eventType), ListenerId(listenerId),
      DispatcherLifetime(std::move(dispatcherLifetime))
{
}

EventSubscription::~EventSubscription()
{
	Reset();
}

EventSubscription::EventSubscription(EventSubscription&& other) noexcept
    : Dispatcher(std::exchange(other.Dispatcher, nullptr)), EventType(std::exchange(other.EventType, nullptr)),
      ListenerId(std::exchange(other.ListenerId, 0)), DispatcherLifetime(std::move(other.DispatcherLifetime))
{
}

EventSubscription& EventSubscription::operator=(EventSubscription&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	Reset();
	Dispatcher = std::exchange(other.Dispatcher, nullptr);
	EventType = std::exchange(other.EventType, nullptr);
	ListenerId = std::exchange(other.ListenerId, 0);
	DispatcherLifetime = std::move(other.DispatcherLifetime);
	return *this;
}

void EventSubscription::Reset() noexcept
{
	if (Dispatcher != nullptr && EventType != nullptr && !DispatcherLifetime.expired())
	{
		Dispatcher->Unsubscribe(*EventType, ListenerId);
	}

	Dispatcher = nullptr;
	EventType = nullptr;
	ListenerId = 0;
	DispatcherLifetime.reset();
}

bool EventSubscription::IsActive() const noexcept
{
	return Dispatcher != nullptr && !DispatcherLifetime.expired();
}
