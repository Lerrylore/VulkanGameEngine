#pragma once

#include <cstdint>
#include <string_view>

class ApplicationConfig final
{
  public:
	static constexpr uint32_t WindowWidth = 800;
	static constexpr uint32_t WindowHeight = 600;
	static constexpr uint32_t ParticleCount = 8192;
	static constexpr uint32_t ComputeWorkgroupSize = 256;
	static constexpr uint32_t MaxFramesInFlight = 2;

	static constexpr std::string_view WindowTitle = "Vulkan Game Engine";
	static constexpr std::string_view ModelPath = "Models/viking_room.obj";
	static constexpr std::string_view BaseColorTexturePath = "Textures/viking_room.png";
	static constexpr std::string_view NormalTexturePath = "Textures/viking_room_normal.png";
	static constexpr std::string_view MetallicRoughnessTexturePath =
		"Textures/viking_room_metallic_roughness.png";

	[[nodiscard]] static constexpr bool EnableValidationLayers() noexcept
	{
#ifdef NDEBUG
		return false;
#else
		return true;
#endif
	}
};
