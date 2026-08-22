#include "Resource.h"

#include <utility>

Resource::Resource(std::string resourceId)
	: ResourceId(std::move(resourceId))
{
}

const std::string& Resource::GetId() const noexcept
{
	return ResourceId;
}

bool Resource::IsLoaded() const noexcept
{
	return Loaded;
}

bool Resource::Load()
{
	Loaded = DoLoad();
	return Loaded;
}

void Resource::Unload()
{
	DoUnload();
	Loaded = false;
}
