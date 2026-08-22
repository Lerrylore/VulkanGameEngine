#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct TextureFile final
{
	int Width = 0;
	int Height = 0;
	std::vector<uint8_t> Pixels;
};

class TextureFileLoader final
{
  public:
	[[nodiscard]] static TextureFile Load(const std::string& path);
};
