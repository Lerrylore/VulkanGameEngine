#include "BinaryFileLoader.h"

#include <fstream>
#include <stdexcept>

std::vector<char> BinaryFileLoader::Load(const std::string& path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		throw std::runtime_error("failed to open binary file: " + path);
	}

	std::vector<char> buffer(static_cast<std::size_t>(file.tellg()));
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
	return buffer;
}
