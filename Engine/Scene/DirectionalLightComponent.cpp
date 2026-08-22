#include "DirectionalLightComponent.h"

#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>

void DirectionalLightComponent::SetDirection(const glm::vec3& direction)
{
	if (glm::length(direction) == 0.0f)
	{
		throw std::invalid_argument("Directional light direction must be non-zero");
	}
	Direction = glm::normalize(direction);
}

void DirectionalLightComponent::SetColor(const glm::vec3& color) noexcept
{
	Color = color;
}

void DirectionalLightComponent::SetIntensity(float intensity)
{
	if (intensity < 0.0f)
	{
		throw std::invalid_argument("Directional light intensity must be non-negative");
	}
	Intensity = intensity;
}

void DirectionalLightComponent::SetAmbientStrength(float ambientStrength)
{
	if (ambientStrength < 0.0f || ambientStrength > 1.0f)
	{
		throw std::invalid_argument("Directional light ambient strength must be between 0 and 1");
	}
	AmbientStrength = ambientStrength;
}

const glm::vec3& DirectionalLightComponent::GetDirection() const noexcept
{
	return Direction;
}

const glm::vec3& DirectionalLightComponent::GetColor() const noexcept
{
	return Color;
}

float DirectionalLightComponent::GetIntensity() const noexcept
{
	return Intensity;
}

float DirectionalLightComponent::GetAmbientStrength() const noexcept
{
	return AmbientStrength;
}

void DirectionalLightComponent::OnInitialize()
{
}

void DirectionalLightComponent::OnDestroy() noexcept
{
}
