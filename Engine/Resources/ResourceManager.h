#pragma once

#include "Resource.h"

#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>

template<typename T>
class ResourceHandle;

struct ResourceKey final
{
	std::type_index Type{typeid(void)};
	std::string Id;

	bool operator==(const ResourceKey& other) const noexcept
	{
		return Type == other.Type && Id == other.Id;
	}
};

struct ResourceKeyHash final
{
	std::size_t operator()(const ResourceKey& key) const noexcept
	{
		const std::size_t typeHash = key.Type.hash_code();
		const std::size_t idHash = std::hash<std::string>{}(key.Id);
		return typeHash ^ (idHash + 0x9e3779b9u + (typeHash << 6) + (typeHash >> 2));
	}
};

class ResourceManager final
{
  public:
	ResourceManager() = default;
	~ResourceManager();

	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;
	ResourceManager(ResourceManager&&) = delete;
	ResourceManager& operator=(ResourceManager&&) = delete;

	template<typename T, typename Factory>
	[[nodiscard]] ResourceHandle<T> LoadWithFactory(const std::string& resourceId, Factory&& factory);

	template<typename T, typename... Arguments>
	[[nodiscard]] ResourceHandle<T> Load(const std::string& resourceId, Arguments&&... arguments)
	{
		return LoadWithFactory<T>(resourceId, [&]()
		{
			return std::make_shared<T>(resourceId, std::forward<Arguments>(arguments)...);
		});
	}

	template<typename T>
	[[nodiscard]] ResourceHandle<T> Adopt(const std::string& resourceId, std::shared_ptr<T> resource);

	template<typename T>
	[[nodiscard]] T* GetResource(const std::string& resourceId) noexcept;

	template<typename T>
	[[nodiscard]] bool HasResource(const std::string& resourceId) const noexcept;

	template<typename T>
	bool Reload(const std::string& resourceId);

	template<typename T>
	void Release(ResourceHandle<T>& handle) noexcept;

	void UnloadAll() noexcept;

  private:
	template<typename T>
	friend class ResourceHandle;

	void AddReference(const ResourceKey& key);
	void Release(const ResourceKey& key) noexcept;

	[[nodiscard]] ResourceKey MakeKey(const std::type_index& type, const std::string& resourceId) const;

	std::unordered_map<ResourceKey, std::shared_ptr<Resource>, ResourceKeyHash> Resources;
	std::unordered_map<ResourceKey, std::size_t, ResourceKeyHash> ReferenceCounts;
};

template<typename T>
class ResourceHandle final
{
  public:
	ResourceHandle() noexcept = default;
	~ResourceHandle();

	ResourceHandle(const ResourceHandle& other);
	ResourceHandle& operator=(const ResourceHandle& other);
	ResourceHandle(ResourceHandle&& other) noexcept;
	ResourceHandle& operator=(ResourceHandle&& other) noexcept;

	[[nodiscard]] T* Get() const noexcept;
	[[nodiscard]] bool IsValid() const noexcept;
	[[nodiscard]] const std::string& GetId() const noexcept;

	void Reset() noexcept;

	[[nodiscard]] T* operator->() const noexcept { return Get(); }
	[[nodiscard]] T& operator*() const { return *Get(); }
	[[nodiscard]] explicit operator bool() const noexcept { return IsValid(); }

  private:
	friend class ResourceManager;

	ResourceHandle(ResourceManager& manager, std::string resourceId) noexcept
		: Manager(&manager), ResourceId(std::move(resourceId))
	{
	}

	ResourceManager* Manager = nullptr;
	std::string ResourceId;
};

template<typename T, typename Factory>
ResourceHandle<T> ResourceManager::LoadWithFactory(const std::string& resourceId, Factory&& factory)
{
	static_assert(std::is_base_of_v<Resource, T>, "T must derive from Resource");
	const ResourceKey key = MakeKey(std::type_index(typeid(T)), resourceId);
	const auto resourceIt = Resources.find(key);
	if (resourceIt != Resources.end())
	{
		AddReference(key);
		return ResourceHandle<T>(*this, resourceId);
	}

	std::shared_ptr<T> resource = factory();
	if (!resource || !resource->Load())
	{
		return {};
	}

	Resources.emplace(key, resource);
	ReferenceCounts.emplace(key, 1);
	return ResourceHandle<T>(*this, resourceId);
}

template<typename T>
ResourceHandle<T> ResourceManager::Adopt(const std::string& resourceId, std::shared_ptr<T> resource)
{
	static_assert(std::is_base_of_v<Resource, T>, "T must derive from Resource");
	if (!resource)
	{
		return {};
	}

	const ResourceKey key = MakeKey(std::type_index(typeid(T)), resourceId);
	const auto resourceIt = Resources.find(key);
	if (resourceIt != Resources.end())
	{
		AddReference(key);
		return ResourceHandle<T>(*this, resourceId);
	}

	if (!resource->IsLoaded() && !resource->Load())
	{
		return {};
	}

	Resources.emplace(key, std::move(resource));
	ReferenceCounts.emplace(key, 1);
	return ResourceHandle<T>(*this, resourceId);
}

template<typename T>
T* ResourceManager::GetResource(const std::string& resourceId) noexcept
{
	static_assert(std::is_base_of_v<Resource, T>, "T must derive from Resource");
	const ResourceKey key = MakeKey(std::type_index(typeid(T)), resourceId);
	const auto resourceIt = Resources.find(key);
	if (resourceIt == Resources.end())
	{
		return nullptr;
	}
	return dynamic_cast<T*>(resourceIt->second.get());
}

template<typename T>
bool ResourceManager::HasResource(const std::string& resourceId) const noexcept
{
	static_assert(std::is_base_of_v<Resource, T>, "T must derive from Resource");
	return Resources.contains(MakeKey(std::type_index(typeid(T)), resourceId));
}

template<typename T>
bool ResourceManager::Reload(const std::string& resourceId)
{
	T* resource = GetResource<T>(resourceId);
	if (resource == nullptr)
	{
		return false;
	}
	resource->Unload();
	return resource->Load();
}

template<typename T>
void ResourceManager::Release(ResourceHandle<T>& handle) noexcept
{
	handle.Reset();
}

template<typename T>
ResourceHandle<T>::~ResourceHandle()
{
	Reset();
}

template<typename T>
ResourceHandle<T>::ResourceHandle(const ResourceHandle& other)
	: Manager(other.Manager), ResourceId(other.ResourceId)
{
	if (Manager != nullptr)
	{
		Manager->AddReference(Manager->MakeKey(std::type_index(typeid(T)), ResourceId));
	}
}

template<typename T>
ResourceHandle<T>& ResourceHandle<T>::operator=(const ResourceHandle& other)
{
	if (this == &other)
	{
		return *this;
	}
	Reset();
	Manager = other.Manager;
	ResourceId = other.ResourceId;
	if (Manager != nullptr)
	{
		Manager->AddReference(Manager->MakeKey(std::type_index(typeid(T)), ResourceId));
	}
	return *this;
}

template<typename T>
ResourceHandle<T>::ResourceHandle(ResourceHandle&& other) noexcept
	: Manager(other.Manager), ResourceId(std::move(other.ResourceId))
{
	other.Manager = nullptr;
}

template<typename T>
ResourceHandle<T>& ResourceHandle<T>::operator=(ResourceHandle&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}
	Reset();
	Manager = other.Manager;
	ResourceId = std::move(other.ResourceId);
	other.Manager = nullptr;
	return *this;
}

template<typename T>
T* ResourceHandle<T>::Get() const noexcept
{
	return Manager == nullptr ? nullptr : Manager->GetResource<T>(ResourceId);
}

template<typename T>
bool ResourceHandle<T>::IsValid() const noexcept
{
	const T* resource = Get();
	return resource != nullptr && resource->IsLoaded();
}

template<typename T>
const std::string& ResourceHandle<T>::GetId() const noexcept
{
	return ResourceId;
}

template<typename T>
void ResourceHandle<T>::Reset() noexcept
{
	if (Manager != nullptr)
	{
		Manager->Release(Manager->MakeKey(std::type_index(typeid(T)), ResourceId));
		Manager = nullptr;
		ResourceId.clear();
	}
}
