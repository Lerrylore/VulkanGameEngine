#include "TextureFileLoader.h"

#include <stb_image.h>

#include <stdexcept>

TextureFile TextureFileLoader::Load(const std::string& path)
{
	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	if (pixels == nullptr)
	{
		throw std::runtime_error("failed to load texture image: " + path);
	}

	TextureFile texture{
		.Width = width,
		.Height = height,
		.Pixels = std::vector<uint8_t>(pixels, pixels + static_cast<std::size_t>(width) * height * 4)};
	stbi_image_free(pixels);
	return texture;
}
