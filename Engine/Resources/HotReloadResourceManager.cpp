#include "HotReloadResourceManager.h"

HotReloadResourceManager::HotReloadResourceManager(ResourceManager& resourceManager)
	: Resources(resourceManager)
{
}

std::filesystem::file_time_type HotReloadResourceManager::GetLastWriteTime(
	const std::filesystem::path& filePath) noexcept
{
	std::error_code error;
	const auto timestamp = std::filesystem::last_write_time(filePath, error);
	return error ? std::filesystem::file_time_type{} : timestamp;
}

std::vector<HotReloadResourceManager::ResourceReloadEvent> HotReloadResourceManager::Poll()
{
	std::vector<ResourceReloadEvent> reloaded;
	for (auto& entry : Watches)
	{
		WatchEntry& watch = entry.second;
		const auto currentTimestamp = GetLastWriteTime(watch.FilePath);
		if (currentTimestamp == std::filesystem::file_time_type{} || currentTimestamp == watch.LastWriteTime)
		{
			continue;
		}

		const bool success = watch.Reload && watch.Reload();
		watch.LastWriteTime = currentTimestamp;
		if (success)
		{
			reloaded.push_back({watch.ResourceId, watch.FilePath});
		}
	}
	return reloaded;
}
