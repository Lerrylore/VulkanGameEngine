#pragma once

#include <cstdint>
#include <memory>
#include <typeinfo>

class EventBus;

class EventSubscription final
{
  public:
	EventSubscription() = default;
	~EventSubscription();

	EventSubscription(const EventSubscription&) = delete;
	EventSubscription& operator=(const EventSubscription&) = delete;
	EventSubscription(EventSubscription&& other) noexcept;
	EventSubscription& operator=(EventSubscription&& other) noexcept;

	void Reset() noexcept;
	[[nodiscard]] bool IsActive() const noexcept;

  private:
	friend class EventBus;

	EventSubscription(EventBus& eventBus, const std::type_info& eventType, uint64_t listenerId,
	                  std::weak_ptr<void> eventBusLifetime) noexcept;

	EventBus* Bus = nullptr;
	const std::type_info* EventType = nullptr;
	uint64_t ListenerId = 0;
	std::weak_ptr<void> BusLifetime;
};
