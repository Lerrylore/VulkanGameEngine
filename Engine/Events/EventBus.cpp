#include "EventBus.h"

#include <algorithm>

EventBus::EventBus() : LifetimeToken(std::make_shared<uint8_t>(0))
{
}

EventBus::~EventBus()
{
	LifetimeToken.reset();
}

bool EventBus::IsDispatching() const noexcept
{
	return DispatchDepth != 0;
}

EventSubscription EventBus::Subscribe(const std::type_info& eventType,
                                             std::function<void(const Event&)> handler)
{
	const uint64_t listenerId = NextListenerId++;
	Listeners[std::type_index(eventType)].push_back(Listener{listenerId, std::move(handler), true});
	return EventSubscription(*this, eventType, listenerId, LifetimeToken);
}

void EventBus::PublishEvent(const std::type_info& eventType, const Event& event)
{
	const std::type_index typeIndex(eventType);
	const auto listeners = Listeners.find(typeIndex);
	if (listeners == Listeners.end())
	{
		return;
	}

	std::vector<uint64_t> listenerIds;
	listenerIds.reserve(listeners->second.size());
	for (const auto& listener : listeners->second)
	{
		if (listener.bActive)
		{
			listenerIds.push_back(listener.Id);
		}
	}

	++DispatchDepth;
	try
	{
		for (const uint64_t listenerId : listenerIds)
		{
			const auto currentListeners = Listeners.find(typeIndex);
			if (currentListeners == Listeners.end())
			{
				break;
			}

			const auto listener = std::find_if(currentListeners->second.begin(), currentListeners->second.end(),
			                                   [listenerId](const Listener& candidate) {
				                                   return candidate.Id == listenerId && candidate.bActive;
			                                   });
			if (listener == currentListeners->second.end())
			{
				continue;
			}

			auto handler = listener->Handler;
			handler(event);
		}
	}
	catch (...)
	{
		--DispatchDepth;
		if (DispatchDepth == 0)
		{
			RemoveAllInactiveListeners();
		}
		throw;
	}

	--DispatchDepth;
	if (DispatchDepth == 0)
	{
		RemoveAllInactiveListeners();
	}
}

void EventBus::Unsubscribe(const std::type_info& eventType, uint64_t listenerId) noexcept
{
	const std::type_index typeIndex(eventType);
	const auto listeners = Listeners.find(typeIndex);
	if (listeners == Listeners.end())
	{
		return;
	}

	const auto listener = std::find_if(listeners->second.begin(), listeners->second.end(),
	                                   [listenerId](const Listener& candidate) { return candidate.Id == listenerId; });
	if (listener == listeners->second.end())
	{
		return;
	}

	listener->bActive = false;
	if (DispatchDepth == 0)
	{
		RemoveInactiveListeners(typeIndex);
	}
}

void EventBus::RemoveInactiveListeners(const std::type_index& eventType)
{
	const auto listeners = Listeners.find(eventType);
	if (listeners == Listeners.end())
	{
		return;
	}

	std::erase_if(listeners->second, [](const Listener& listener) { return !listener.bActive; });
	if (listeners->second.empty())
	{
		Listeners.erase(listeners);
	}
}

void EventBus::RemoveAllInactiveListeners()
{
	for (auto listeners = Listeners.begin(); listeners != Listeners.end();)
	{
		std::erase_if(listeners->second, [](const Listener& listener) { return !listener.bActive; });
		if (listeners->second.empty())
		{
			listeners = Listeners.erase(listeners);
		}
		else
		{
			++listeners;
		}
	}
}
