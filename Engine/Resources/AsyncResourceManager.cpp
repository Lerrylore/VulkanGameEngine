#include "AsyncResourceManager.h"

AsyncResourceManager::AsyncResourceManager(ResourceManager& resourceManager)
	: Resources(resourceManager), Worker(&AsyncResourceManager::WorkerThread, this)
{
}

AsyncResourceManager::~AsyncResourceManager()
{
	Stop();
}

void AsyncResourceManager::Stop()
{
	{
		std::lock_guard lock(QueueMutex);
		if (!Running)
		{
			return;
		}
		Running = false;
	}
	Condition.notify_one();
	if (Worker.joinable())
	{
		Worker.join();
	}
}

void AsyncResourceManager::Enqueue(std::function<void()> task)
{
	{
		std::lock_guard lock(QueueMutex);
		if (!Running)
		{
			return;
		}
		Tasks.push(std::move(task));
	}
	Condition.notify_one();
}

void AsyncResourceManager::WorkerThread()
{
	while (true)
	{
		std::function<void()> task;
		{
			std::unique_lock lock(QueueMutex);
			Condition.wait(lock, [this]()
			{
				return !Tasks.empty() || !Running;
			});

			if (!Running && Tasks.empty())
			{
				return;
			}
			task = std::move(Tasks.front());
			Tasks.pop();
		}
		task();
	}
}
