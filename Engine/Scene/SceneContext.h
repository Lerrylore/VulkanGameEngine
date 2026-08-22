#pragma once

class EventBus;
class ServiceLocator;

class SceneContext final
{
  public:
	explicit SceneContext(ServiceLocator& serviceLocator) noexcept;

	[[nodiscard]] EventBus& GetEventBus();
	[[nodiscard]] const EventBus& GetEventBus() const;

  private:
	ServiceLocator& Services;
};
