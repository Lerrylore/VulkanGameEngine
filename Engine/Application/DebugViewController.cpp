#include "DebugViewController.h"

#include "../Platform/Window.h"

#include <array>
#include <iostream>
#include <utility>

void DebugViewController::Update(const Window& window, DebugViewMode& currentMode)
{
	static constexpr std::array<std::pair<WindowKey, DebugViewMode>, 15> modes{{
		{WindowKey::Number0, DebugViewMode::Lit},
		{WindowKey::Number1, DebugViewMode::WorldNormal},
		{WindowKey::Number2, DebugViewMode::TangentNormal},
		{WindowKey::Number3, DebugViewMode::Roughness},
		{WindowKey::Number4, DebugViewMode::Metallic},
		{WindowKey::Number5, DebugViewMode::Shadow},
		{WindowKey::Number6, DebugViewMode::Albedo},
		{WindowKey::Number7, DebugViewMode::GeometricNormal},
		{WindowKey::Number8, DebugViewMode::Tangent},
		{WindowKey::Number9, DebugViewMode::ShadowDepth},
		{WindowKey::F1, DebugViewMode::Diffuse},
		{WindowKey::F2, DebugViewMode::Specular},
		{WindowKey::F3, DebugViewMode::Fresnel},
		{WindowKey::F4, DebugViewMode::AmbientOcclusion},
		{WindowKey::F5, DebugViewMode::Emissive}}};

	for (const auto [key, mode] : modes)
	{
		if (window.IsKeyDown(key))
		{
			if (currentMode != mode)
			{
				currentMode = mode;
				std::clog << "[DebugView] mode " << static_cast<uint32_t>(mode) << '\n';
			}
			return;
		}
	}
}

void DebugViewController::PrintHelp()
{
	std::clog << "[DebugView] 0 Lit, 1 WorldNormal, 2 TangentNormal, 3 Roughness, 4 Metallic, "
		"5 Shadow, 6 Albedo, 7 GeometricNormal, 8 Tangent, 9 ShadowDepth, "
		"F1 Diffuse, F2 Specular, F3 Fresnel, F4 AO, F5 Emissive\n";
}
