#pragma once

#include <memory>
#include <vector>

class GameObject;

class Scene final
{
  public:
	Scene() = default;
	~Scene();

	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&) = delete;
	Scene& operator=(Scene&&) = delete;

	GameObject& createGameObject();

  private:
	std::vector<std::unique_ptr<GameObject>> gameObjects_;
};
