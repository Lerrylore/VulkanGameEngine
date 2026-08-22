#pragma once

#include <string>
#include <vector>

class BinaryFileLoader final
{
  public:
	[[nodiscard]] static std::vector<char> Load(const std::string& path);
};
