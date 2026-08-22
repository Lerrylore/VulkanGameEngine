#include "ResourceManager.h"

#include <stdexcept>

ResourceManager::~ResourceManager()
{
	UnloadAll();
}

void ResourceManager::AddReference(const ResourceKey& key)
{
	const auto referenceIt = ReferenceCounts.find(key);
	if (referenceIt == ReferenceCounts.end())
	{
		throw std::logic_error("cannot reference a resource that is not cached");
	}
	++referenceIt->second;
}

void ResourceManager::Release(const ResourceKey& key) noexcept
{
	const auto referenceIt = ReferenceCounts.find(key);
	if (referenceIt == ReferenceCounts.end())
	{
		return;
	}

	if (referenceIt->second > 1)
	{
		--referenceIt->second;
		return;
	}

	const auto resourceIt = Resources.find(key);
	if (resourceIt != Resources.end())
	{
		resourceIt->second->Unload();
		Resources.erase(resourceIt);
	}
	ReferenceCounts.erase(referenceIt);
}

ResourceKey ResourceManager::MakeKey(
	const std::type_index& type,
	const std::string& resourceId) const
{
	return ResourceKey{type, resourceId};
}

void ResourceManager::UnloadAll() noexcept
{
	for (auto& [key, resource] : Resources)
	{
		resource->Unload();
	}
	Resources.clear();
	ReferenceCounts.clear();
}
