#pragma once

class GameObject;

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

	virtual void OnInitialize();
	virtual void OnUpdate(float deltaTime);
	virtual void OnDestroy() noexcept;

  private:
	friend class GameObject;

	void Attach(GameObject& owner) noexcept;
	void Initialize();
	void Update(float deltaTime);
	void Destroy() noexcept;

	GameObject* Owner = nullptr;
	State CurrentState = State::Uninitialized;
};
