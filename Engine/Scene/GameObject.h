#pragma once

#include "Component.h"

#include <memory>
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

		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		auto& result = *component;
		components_.push_back(std::move(component));
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

  private:
	std::vector<std::unique_ptr<Component>> components_;
	TransformComponent* transform_ = nullptr;
};
