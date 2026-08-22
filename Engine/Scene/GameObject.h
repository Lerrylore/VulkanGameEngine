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
	T& AddComponent(Args&&... args)
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
		if (bDestroyed)
		{
			throw std::logic_error("Cannot add a component to a destroyed GameObject");
		}

		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		component->Attach(*this);
		auto& result = *component;
		const size_t componentCountBeforeInsertion = Components.size();
		Components.push_back(std::move(component));

		if (bInitialized)
		{
			try
			{
				result.Initialize();
			}
			catch (...)
			{
				while (Components.size() > componentCountBeforeInsertion)
				{
					Components.back()->Destroy();
					Components.pop_back();
				}
				throw;
			}
		}
		return result;
	}

	template <typename T>
	[[nodiscard]] T* GetComponent() noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

		for (const auto& component : Components)
		{
			if (auto* result = dynamic_cast<T*>(component.get()))
			{
				return result;
			}
		}
		return nullptr;
	}

	template <typename T>
	[[nodiscard]] const T* GetComponent() const noexcept
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

		for (const auto& component : Components)
		{
			if (const auto* result = dynamic_cast<const T*>(component.get()))
			{
				return result;
			}
		}
		return nullptr;
	}

	[[nodiscard]] TransformComponent& GetTransform() noexcept;
	[[nodiscard]] const TransformComponent& GetTransform() const noexcept;
	[[nodiscard]] bool IsInitialized() const noexcept;

	void Initialize();
	void Update(float deltaTime);
	void Destroy() noexcept;

  private:
	std::vector<std::unique_ptr<Component>> Components;
	TransformComponent* Transform = nullptr;
	bool bInitialized = false;
	bool bDestroyed = false;
};
