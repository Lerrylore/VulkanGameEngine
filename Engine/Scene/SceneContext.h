#pragma once

class EventDispatcher;

class SceneContext final
{
  public:
	explicit SceneContext(EventDispatcher& eventDispatcher) noexcept;

	[[nodiscard]] EventDispatcher& GetEventDispatcher() noexcept;
	[[nodiscard]] const EventDispatcher& GetEventDispatcher() const noexcept;

  private:
	EventDispatcher& Events;
};
