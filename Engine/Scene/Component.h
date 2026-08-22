#pragma once

#include "../Events/EventBus.h"

#include <functional>
#include <utility>
#include <vector>

class GameObject;
class SceneContext;

class Component
{
  public:
	enum class State
	{
		Uninitialized,
		Initializing,
		Active,
		Destroying,
		Destroyed
	};

	virtual ~Component();

	[[nodiscard]] State GetState() const noexcept;
	[[nodiscard]] bool IsActive() const noexcept;

  protected:
	Component() = default;

	[[nodiscard]] GameObject& GetOwner() noexcept;
	[[nodiscard]] const GameObject& GetOwner() const noexcept;
	[[nodiscard]] EventBus& GetEventBus();
	[[nodiscard]] const EventBus& GetEventBus() const;

	template <typename TEvent, typename Handler>
	void Listen(Handler&& handler)
	{
		auto guardedHandler = [this, handler = std::forward<Handler>(handler)](const TEvent& event) mutable
		{
			if (IsActive())
			{
				std::invoke(handler, event);
			}
		};
		EventSubscriptions.push_back(
			GetEventBus().Subscribe<TEvent>(std::move(guardedHandler)));
	}

	virtual void OnInitialize();
	virtual void OnUpdate(float deltaTime);
	virtual void OnDestroy() noexcept;

  private:
	friend class GameObject;

	void Attach(GameObject& owner) noexcept;
	void Initialize(SceneContext& context);
	void Update(float deltaTime);
	void Destroy() noexcept;
	void ResetEventSubscriptions() noexcept;

	std::vector<EventSubscription> EventSubscriptions;
	GameObject* Owner = nullptr;
	SceneContext* Context = nullptr;
	State CurrentState = State::Uninitialized;
};
