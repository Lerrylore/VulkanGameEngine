#include "ResourceStreamingManager.h"

#include <utility>

void ResourceStreamingManager::RequestChunk(
	std::string resourceId,
	uint32_t chunkIndex,
	uint32_t priority,
	ChunkLoader loader)
{
	Requests.push(Request{std::move(resourceId), chunkIndex, priority, std::move(loader)});
}

uint32_t ResourceStreamingManager::Process(uint32_t maxChunks)
{
	uint32_t processed = 0;
	while (processed < maxChunks && !Requests.empty())
	{
		Request request = Requests.top();
		Requests.pop();
		if (request.Loader)
		{
			request.Loader(request.ChunkIndex);
		}
		++processed;
	}
	return processed;
}

std::size_t ResourceStreamingManager::PendingRequestCount() const noexcept
{
	return Requests.size();
}
