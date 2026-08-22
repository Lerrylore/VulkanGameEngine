#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <string>

class ResourceStreamingManager final
{
  public:
	using ChunkLoader = std::function<void(uint32_t chunkIndex)>;

	void RequestChunk(std::string resourceId, uint32_t chunkIndex, uint32_t priority, ChunkLoader loader);
	uint32_t Process(uint32_t maxChunks);

	[[nodiscard]] std::size_t PendingRequestCount() const noexcept;

  private:
	struct Request final
	{
		std::string ResourceId;
		uint32_t ChunkIndex = 0;
		uint32_t Priority = 0;
		ChunkLoader Loader;

		bool operator<(const Request& other) const noexcept
		{
			return Priority < other.Priority;
		}
	};

	std::priority_queue<Request> Requests;
};
