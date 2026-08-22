#pragma once

#include "Component.h"

#include <glm/glm.hpp>

class TransformComponent;

class CameraComponent final : public Component
{
  public:
	void SetTarget(const glm::vec3& target) noexcept;
	void SetUp(const glm::vec3& up) noexcept;
	void SetFieldOfView(float fieldOfViewDegrees);
	void SetAspectRatio(float aspectRatio);
	void SetClipPlanes(float nearPlane, float farPlane);

	[[nodiscard]] const glm::vec3& GetTarget() const noexcept;
	[[nodiscard]] const glm::vec3& GetUp() const noexcept;
	[[nodiscard]] float GetFieldOfView() const noexcept;
	[[nodiscard]] float GetAspectRatio() const noexcept;
	[[nodiscard]] float GetNearPlane() const noexcept;
	[[nodiscard]] float GetFarPlane() const noexcept;
	[[nodiscard]] const glm::vec3& GetPosition() const noexcept;
	[[nodiscard]] glm::mat4 GetViewMatrix() const;
	[[nodiscard]] glm::mat4 GetProjectionMatrix() const;

  protected:
	void OnInitialize() override;
	void OnDestroy() noexcept override;

  private:
	TransformComponent* Transform = nullptr;
	glm::vec3 Target{0.0f};
	glm::vec3 Up{0.0f, 0.0f, 1.0f};
	float FieldOfViewDegrees = 45.0f;
	float AspectRatio = 4.0f / 3.0f;
	float NearPlane = 0.1f;
	float FarPlane = 10.0f;
};
