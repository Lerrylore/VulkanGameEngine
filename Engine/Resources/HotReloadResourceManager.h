#pragma once

#include "ResourceManager.h"

#include <filesystem>
#include <string>
#include <functional>
#include <unordered_map>

class HotReloadResourceManager final
{
  public:
	explicit HotReloadResourceManager(ResourceManager& resourceManager);

	 template<typename T>
	 void Watch(const ResourceHandle<T>& handle, std::filesystem::path filePath)
	 {
		WatchEntry entry;
		entry.ResourceId = handle.GetId();
		entry.FilePath = std::move(filePath);
		entry.LastWriteTime = GetLastWriteTime(entry.FilePath);
		entry.Reload = [this, resourceId = entry.ResourceId]()
		{
			return Resources.Reload<T>(resourceId);
		};
		Watches[entry.FilePath.string()] = std::move(entry);
	 }

	 uint32_t Poll();

  private:
	struct WatchEntry final
	 {
		std::string ResourceId;
		std::filesystem::path FilePath;
		std::filesystem::file_time_type LastWriteTime{};
		std::function<bool()> Reload;
	};

	static std::filesystem::file_time_type GetLastWriteTime(const std::filesystem::path& filePath) noexcept;

	 ResourceManager& Resources;
	 std::unordered_map<std::string, WatchEntry> Watches;
};
