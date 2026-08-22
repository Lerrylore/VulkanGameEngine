#pragma once

#include "Events/EventDispatcher.h"
#include "SceneContext.h"

#include <memory>
#include <vector>

class GameObject;

class Scene final
{
  public:
	Scene();
	~Scene();

	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&) = delete;
	Scene& operator=(Scene&&) = delete;

	GameObject& CreateGameObject();
	[[nodiscard]] bool IsInitialized() const noexcept;

	template <typename TEvent>
	void Publish(const TEvent& event)
	{
		Events.Publish(event);
	}

	void Initialize();
	void Update(float deltaTime);
	void Destroy() noexcept;

  private:
	EventDispatcher Events;
	SceneContext Context;
	std::vector<std::unique_ptr<GameObject>> GameObjects;
	bool bInitialized = false;
	bool bDestroyed = false;
};
