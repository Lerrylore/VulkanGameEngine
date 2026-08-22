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

	[[nodiscard]] State state() const noexcept;
	[[nodiscard]] bool isActive() const noexcept;

  protected:
	Component() = default;

	[[nodiscard]] GameObject& owner() noexcept;
	[[nodiscard]] const GameObject& owner() const noexcept;

	virtual void onInitialize();
	virtual void onUpdate(float deltaTime);
	virtual void onDestroy() noexcept;

  private:
	friend class GameObject;

	void attach(GameObject& owner) noexcept;
	void initialize();
	void update(float deltaTime);
	void destroy() noexcept;

	GameObject* owner_ = nullptr;
	State state_ = State::Uninitialized;
};
