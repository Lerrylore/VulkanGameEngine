#include "ResourceStreamingManager.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <utility>

ResourceStreamingManager::RequestId ResourceStreamingManager::RequestFileChunk(
	std::string resourceId,
	std::filesystem::path filePath,
	std::uint32_t chunkIndex,
	std::uint32_t chunkSize,
	std::uint32_t priority,
	ChunkCallback callback)
{
	std::lock_guard lock(QueueMutex);

	const RequestId requestId = NextRequestId++;
	Requests.push(Request{
		requestId,
		std::move(resourceId),
		std::move(filePath),
		chunkIndex,
		chunkSize,
		priority,
		NextSequence++,
		std::move(callback)});
	return requestId;
}

std::uint32_t ResourceStreamingManager::Process(std::uint32_t maxChunks)
{
	std::lock_guard processLock(ProcessMutex);

	std::uint32_t processed = 0;
	while (processed < maxChunks)
	{
		Request request;
		{
			std::lock_guard queueLock(QueueMutex);
			if (Requests.empty())
			{
				break;
			}

			request = Requests.top();
			Requests.pop();
		}

		ChunkResult result = LoadChunk(request);
		if (request.Callback)
		{
			request.Callback(std::move(result));
		}
		++processed;
	}

	return processed;
}

std::size_t ResourceStreamingManager::PendingRequestCount() const noexcept
{
	std::lock_guard lock(QueueMutex);
	return Requests.size();
}

ResourceStreamingManager::ChunkResult ResourceStreamingManager::LoadChunk(const Request& request)
{
	ChunkResult result;
	result.Id = request.Id;
	result.Data.ResourceId = request.ResourceId;
	result.Data.ChunkIndex = request.ChunkIndex;

	if (request.ChunkSize == 0)
	{
		result.Error = "Chunk size must be greater than zero";
		return result;
	}

	std::ifstream file(request.FilePath, std::ios::binary | std::ios::ate);
	if (!file)
	{
		result.Error = "Could not open file: " + request.FilePath.string();
		return result;
	}

	const std::streampos endPosition = file.tellg();
	if (endPosition < 0)
	{
		result.Error = "Could not determine file size: " + request.FilePath.string();
		return result;
	}

	const auto fileSize = static_cast<std::uintmax_t>(endPosition);
	const auto chunkSize = static_cast<std::uintmax_t>(request.ChunkSize);
	const auto chunkIndex = static_cast<std::uintmax_t>(request.ChunkIndex);
	if (chunkIndex > std::numeric_limits<std::uintmax_t>::max() / chunkSize)
	{
		result.Error = "Chunk offset overflow";
		return result;
	}

	const auto offset = chunkIndex * chunkSize;
	if (fileSize == 0 && offset == 0)
	{
		result.Data.Offset = 0;
		result.Data.IsFinalChunk = true;
		return result;
	}
	if (offset >= fileSize)
	{
		result.Error = "Chunk is outside the file: " + request.FilePath.string();
		return result;
	}

	const auto bytesToRead = std::min(chunkSize, fileSize - offset);
	result.Data.Offset = static_cast<std::uint64_t>(offset);
	result.Data.IsFinalChunk = bytesToRead == fileSize - offset;
	result.Data.Bytes.resize(static_cast<std::size_t>(bytesToRead));

	file.seekg(static_cast<std::streamoff>(offset));
	file.read(
		reinterpret_cast<char*>(result.Data.Bytes.data()),
		static_cast<std::streamsize>(result.Data.Bytes.size()));
	if (file.gcount() != static_cast<std::streamsize>(result.Data.Bytes.size()))
	{
		result.Data.Bytes.clear();
		result.Error = "Could not read chunk from file: " + request.FilePath.string();
	}

	return result;
}
