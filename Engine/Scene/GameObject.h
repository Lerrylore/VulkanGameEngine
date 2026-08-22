#pragma once

#include "Component.h"

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

class TransformComponent;

class GameObject final
{
  public:
	GameObject();
	~GameObject();

	GameObject(const GameObject&) = delete;
	GameObject& operator=(const GameObject&) = delete;
	GameObject(GameObject&&) = delete;
	GameObject& operator=(GameObject&&) = delete;

	template <typename T, typename... Args>
	T& addComponent(Args&&... args)
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
		if (destroyed_)
		{
			throw std::logic_error("Cannot add a component to a destroyed GameObject");
		}

		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		component->attach(*this);
		auto& result = *component;
		const size_t componentCountBeforeInsertion = components_.size();
		components_.push_back(std::move(component));

		if (initialized_)
		{
			try
			{
				result.initialize();
			}
			catch (...)
			{
				while (components_.size() > componentCountBeforeInsertion)
				{
					components_.back()->destroy();
					components_.pop_back();
				}
				throw;
			}
		}
		return result;
	}

	template <typename T>
	[[nodiscard]] T* getComponent() noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

		for (const auto& component : components_)
		{
			if (auto* result = dynamic_cast<T*>(component.get()))
			{
				return result;
			}
		}
		return nullptr;
	}

	template <typename T>
	[[nodiscard]] const T* getComponent() const noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

		for (const auto& component : components_)
		{
			if (const auto* result = dynamic_cast<const T*>(component.get()))
			{
				return result;
			}
		}
		return nullptr;
	}

	[[nodiscard]] TransformComponent& transform() noexcept;
	[[nodiscard]] const TransformComponent& transform() const noexcept;
	[[nodiscard]] bool isInitialized() const noexcept;

	void initialize();
	void update(float deltaTime);
	void destroy() noexcept;

  private:
	std::vector<std::unique_ptr<Component>> components_;
	TransformComponent* transform_ = nullptr;
	bool initialized_ = false;
	bool destroyed_ = false;
};
