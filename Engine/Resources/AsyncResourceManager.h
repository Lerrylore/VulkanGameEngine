#pragma once

#include "ResourceManager.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

class AsyncResourceManager final
{
  public:
	explicit AsyncResourceManager(ResourceManager& resourceManager);
	~AsyncResourceManager();

	AsyncResourceManager(const AsyncResourceManager&) = delete;
	AsyncResourceManager& operator=(const AsyncResourceManager&) = delete;

	template<typename T, typename Factory, typename Callback>
	void LoadAsync(const std::string& resourceId, Factory factory, Callback callback)
	{
		Enqueue([this, resourceId, factory = std::move(factory), callback = std::move(callback)]() mutable
		{
			ResourceHandle<T> handle = Resources.LoadWithFactory<T>(resourceId, factory);
			callback(std::move(handle));
		});
	}

	void Stop();

  private:
	void Enqueue(std::function<void()> task);
	void WorkerThread();

	ResourceManager& Resources;
	std::thread Worker;
	std::mutex QueueMutex;
	std::condition_variable Condition;
	std::queue<std::function<void()>> Tasks;
	bool Running = true;
};
