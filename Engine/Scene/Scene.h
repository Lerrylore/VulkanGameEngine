#pragma once

#include "../Events/EventBus.h"
#include "GameObject.h"
#include "SceneContext.h"

#include <memory>
#include <vector>

class ServiceLocator;

class Scene final
{
  public:
	explicit Scene(ServiceLocator& serviceLocator) noexcept;
	~Scene();

	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&) = delete;
	Scene& operator=(Scene&&) = delete;

	GameObject& CreateGameObject();
	[[nodiscard]] bool IsInitialized() const noexcept;

	template <typename T, typename TCallback>
	void ForEachComponent(TCallback&& callback)
	{
		for (const auto& gameObject : GameObjects)
		{
			for (T* component : gameObject->GetComponents<T>())
			{
				callback(*component);
			}
		}
	}

	template <typename T, typename TCallback>
	void ForEachComponent(TCallback&& callback) const
	{
		for (const auto& gameObject : GameObjects)
		{
			const GameObject& constGameObject = *gameObject;
			for (const T* component : constGameObject.GetComponents<T>())
			{
				callback(*component);
			}
		}
	}

	template <typename TEvent>
	void PublishEvent(const TEvent& event)
	{
		Context.GetEventBus().PublishEvent(event);
	}

	void Initialize();
	void Update(float deltaTime);
	void Destroy() noexcept;

  private:
	SceneContext Context;
	std::vector<std::unique_ptr<GameObject>> GameObjects;
	bool bInitialized = false;
	bool bDestroyed = false;
};
