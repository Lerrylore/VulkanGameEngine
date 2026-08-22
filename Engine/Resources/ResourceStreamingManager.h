#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

class ResourceStreamingManager final
{
  public:
	using RequestId = std::uint64_t;

	struct ChunkData final
	{
		std::string ResourceId;
		std::uint32_t ChunkIndex = 0;
		std::uint64_t Offset = 0;
		bool IsFinalChunk = false;
		std::vector<std::byte> Bytes;
	};

	struct ChunkResult final
	{
		RequestId Id = 0;
		ChunkData Data;
		std::string Error;

		[[nodiscard]] bool Succeeded() const noexcept
		{
			return Error.empty();
		}
	};

	// RequestFileChunk only queues work. The manager owns the request and the
	// callback until Process removes it from the queue. The callback receives
	// the CPU buffer by value and therefore owns its copy after it returns.
	using ChunkCallback = std::function<void(ChunkResult)>;

	ResourceStreamingManager() = default;
	ResourceStreamingManager(const ResourceStreamingManager&) = delete;
	ResourceStreamingManager& operator=(const ResourceStreamingManager&) = delete;

	RequestId RequestFileChunk(
		std::string resourceId,
		std::filesystem::path filePath,
		std::uint32_t chunkIndex,
		std::uint32_t chunkSize,
		std::uint32_t priority,
		ChunkCallback callback);

	// Processes at most maxChunks requests on the calling thread. Requests may
	// be submitted concurrently, but callbacks are serialized by Process.
	std::uint32_t Process(std::uint32_t maxChunks);

	[[nodiscard]] std::size_t PendingRequestCount() const noexcept;

  private:
	struct Request final
	{
		RequestId Id = 0;
		std::string ResourceId;
		std::filesystem::path FilePath;
		std::uint32_t ChunkIndex = 0;
		std::uint32_t ChunkSize = 0;
		std::uint32_t Priority = 0;
		std::uint64_t Sequence = 0;
		ChunkCallback Callback;

		bool operator<(const Request& other) const noexcept
		{
			if (Priority != other.Priority)
			{
				return Priority < other.Priority;
			}

			// For equal priorities, preserve FIFO order. priority_queue puts the
			// request for which operator< returns false at the top.
			return Sequence > other.Sequence;
		}
	};

	[[nodiscard]] static ChunkResult LoadChunk(const Request& request);

	mutable std::mutex QueueMutex;
	std::mutex ProcessMutex;
	std::priority_queue<Request> Requests;
	RequestId NextRequestId = 1;
	std::uint64_t NextSequence = 0;
};
