#pragma once

#include "Event.h"
#include "EventSubscription.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

class EventBus final
{
  public:
	EventBus();
	~EventBus();

	EventBus(const EventBus&) = delete;
	EventBus& operator=(const EventBus&) = delete;
	EventBus(EventBus&&) = delete;
	EventBus& operator=(EventBus&&) = delete;

	template <typename TEvent, typename Handler>
	EventSubscription Subscribe(Handler&& handler)
	{
		static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Event");

		std::function<void(const TEvent&)> typedHandler(std::forward<Handler>(handler));
		return Subscribe(typeid(TEvent),
		                 [handler = std::move(typedHandler)](const Event& event) { handler(static_cast<const TEvent&>(event)); });
	}

	template <typename TEvent>
	void PublishEvent(const TEvent& event)
	{
		static_assert(std::is_base_of_v<Event, TEvent>, "TEvent must derive from Event");
		PublishEvent(typeid(TEvent), event);
	}

	[[nodiscard]] bool IsDispatching() const noexcept;

  private:
	friend class EventSubscription;

	struct Listener final
	{
		uint64_t Id = 0;
		std::function<void(const Event&)> Handler;
		bool bActive = true;
	};

	EventSubscription Subscribe(const std::type_info& eventType, std::function<void(const Event&)> handler);
	void PublishEvent(const std::type_info& eventType, const Event& event);
	void Unsubscribe(const std::type_info& eventType, uint64_t listenerId) noexcept;
	void RemoveInactiveListeners(const std::type_index& eventType);
	void RemoveAllInactiveListeners();

	std::unordered_map<std::type_index, std::vector<Listener>> Listeners;
	std::shared_ptr<void> LifetimeToken;
	uint64_t NextListenerId = 1;
	uint32_t DispatchDepth = 0;
};
