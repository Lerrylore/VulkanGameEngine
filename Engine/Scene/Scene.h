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

	GameObject& CreateGameObject();
	[[nodiscard]] bool IsInitialized() const noexcept;

	void Initialize();
	void Update(float deltaTime);
	void Destroy() noexcept;

  private:
	std::vector<std::unique_ptr<GameObject>> GameObjects;
	bool bInitialized = false;
	bool bDestroyed = false;
};
