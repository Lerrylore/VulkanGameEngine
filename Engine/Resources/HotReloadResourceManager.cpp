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

uint32_t HotReloadResourceManager::Poll()
{
	uint32_t reloaded = 0;
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
			++reloaded;
			if (watch.OnReload)
			{
				watch.OnReload();
			}
		}
	}
	return reloaded;
}
