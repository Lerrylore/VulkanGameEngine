#include "BinaryResource.h"

#include "BinaryFileLoader.h"

#include <exception>
#include <utility>

BinaryResource::BinaryResource(std::string resourceId, std::string filePath)
	: Resource(std::move(resourceId)), FilePath(std::move(filePath))
{
}

const std::vector<char>& BinaryResource::data() const noexcept
{
	return Data;
}

const std::string& BinaryResource::filePath() const noexcept
{
	return FilePath;
}

bool BinaryResource::DoLoad()
{
	try
	{
		Data = BinaryFileLoader::Load(FilePath);
		return !Data.empty();
	}
	catch (const std::exception&)
	{
		Data.clear();
		return false;
	}
}

void BinaryResource::DoUnload()
{
	Data.clear();
}
