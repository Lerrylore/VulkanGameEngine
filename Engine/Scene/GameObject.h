#pragma once

#include "Component.h"

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

class TransformComponent;
class SceneContext;

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
		if constexpr (std::is_same_v<T, TransformComponent>)
		{
			if (HasComponent<TransformComponent>())
			{
				throw std::logic_error("A GameObject can only have one TransformComponent");
			}
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
				result.Initialize(*Context);
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
			if (!IsPendingRemoval(*component))
			{
				if (auto* result = dynamic_cast<T*>(component.get()))
				{
					return result;
				}
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
			if (!IsPendingRemoval(*component))
			{
				if (const auto* result = dynamic_cast<const T*>(component.get()))
				{
					return result;
				}
			}
		}
		return nullptr;
	}

	template <typename T>
	[[nodiscard]] bool HasComponent() const noexcept
	{
		return GetComponent<T>() != nullptr;
	}

	template <typename T>
	[[nodiscard]] std::vector<T*> GetComponents()
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

		std::vector<T*> results;
		for (const auto& component : Components)
		{
			if (!IsPendingRemoval(*component))
			{
				if (auto* result = dynamic_cast<T*>(component.get()))
				{
					results.push_back(result);
				}
			}
		}
		return results;
	}

	template <typename T>
	[[nodiscard]] std::vector<const T*> GetComponents() const
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

		std::vector<const T*> results;
		for (const auto& component : Components)
		{
			if (!IsPendingRemoval(*component))
			{
				if (const auto* result = dynamic_cast<const T*>(component.get()))
				{
					results.push_back(result);
				}
			}
		}
		return results;
	}

	template <typename T>
	[[nodiscard]] T& GetRequiredComponent()
	{
		if (auto* component = GetComponent<T>())
		{
			return *component;
		}
		throw std::logic_error("Required component is missing");
	}

	template <typename T>
	[[nodiscard]] const T& GetRequiredComponent() const
	{
		if (const auto* component = GetComponent<T>())
		{
			return *component;
		}
		throw std::logic_error("Required component is missing");
	}

	template <typename T>
	bool RemoveComponent()
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
		if (bDestroyed)
		{
			throw std::logic_error("Cannot remove a component from a destroyed GameObject");
		}

		if (auto* component = GetComponent<T>())
		{
			return RemoveComponent(*component);
		}
		return false;
	}

	bool RemoveComponent(Component& component);

	[[nodiscard]] TransformComponent& GetTransform() noexcept;
	[[nodiscard]] const TransformComponent& GetTransform() const noexcept;
	[[nodiscard]] bool IsInitialized() const noexcept;

	void Initialize(SceneContext& context);
	void Update(float deltaTime);
	void Destroy() noexcept;

  private:
	[[nodiscard]] bool IsPendingRemoval(const Component& component) const noexcept;
	void RemovePendingComponents() noexcept;

	std::vector<std::unique_ptr<Component>> Components;
	std::vector<Component*> PendingComponentRemovals;
	TransformComponent* Transform = nullptr;
	SceneContext* Context = nullptr;
	bool bInitialized = false;
	bool bDestroyed = false;
	bool bUpdating = false;
};
