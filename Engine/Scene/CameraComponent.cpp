#include "CameraComponent.h"

#include "GameObject.h"
#include "TransformComponent.h"

#include <cassert>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

void CameraComponent::SetTarget(const glm::vec3& target) noexcept
{
	Target = target;
}

void CameraComponent::SetUp(const glm::vec3& up) noexcept
{
	Up = up;
}

void CameraComponent::SetFieldOfView(float fieldOfViewDegrees)
{
	if (fieldOfViewDegrees <= 0.0f || fieldOfViewDegrees >= 180.0f)
	{
		throw std::invalid_argument("Camera field of view must be between 0 and 180 degrees");
	}
	FieldOfViewDegrees = fieldOfViewDegrees;
}

void CameraComponent::SetAspectRatio(float aspectRatio)
{
	if (aspectRatio <= 0.0f)
	{
		throw std::invalid_argument("Camera aspect ratio must be positive");
	}
	AspectRatio = aspectRatio;
}

void CameraComponent::SetClipPlanes(float nearPlane, float farPlane)
{
	if (nearPlane <= 0.0f || farPlane <= nearPlane)
	{
		throw std::invalid_argument("Camera clip planes must satisfy 0 < near < far");
	}
	NearPlane = nearPlane;
	FarPlane = farPlane;
}

const glm::vec3& CameraComponent::GetTarget() const noexcept
{
	return Target;
}

const glm::vec3& CameraComponent::GetUp() const noexcept
{
	return Up;
}

float CameraComponent::GetFieldOfView() const noexcept
{
	return FieldOfViewDegrees;
}

float CameraComponent::GetAspectRatio() const noexcept
{
	return AspectRatio;
}

float CameraComponent::GetNearPlane() const noexcept
{
	return NearPlane;
}

float CameraComponent::GetFarPlane() const noexcept
{
	return FarPlane;
}

const glm::vec3& CameraComponent::GetPosition() const noexcept
{
	assert(Transform != nullptr);
	return Transform->GetPosition();
}

glm::mat4 CameraComponent::GetViewMatrix() const
{
	assert(Transform != nullptr);
	return glm::lookAt(Transform->GetPosition(), Target, Up);
}

glm::mat4 CameraComponent::GetProjectionMatrix() const
{
	glm::mat4 projection = glm::perspective(
		glm::radians(FieldOfViewDegrees),
		AspectRatio,
		NearPlane,
		FarPlane);
	projection[1][1] *= -1.0f;
	return projection;
}

void CameraComponent::OnInitialize()
{
	Transform = &GetOwner().GetRequiredComponent<TransformComponent>();
	if (!Transform->IsActive())
	{
		Transform = nullptr;
		throw std::logic_error("CameraComponent requires an active TransformComponent");
	}
}

void CameraComponent::OnDestroy() noexcept
{
	Transform = nullptr;
}
