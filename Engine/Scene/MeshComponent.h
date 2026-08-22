#pragma once

#include "Component.h"

class MeshResource;
class TransformComponent;

class MeshComponent final : public Component
{
  public:
	explicit MeshComponent(MeshResource& mesh) noexcept;

	[[nodiscard]] MeshResource& GetMesh() noexcept;
	[[nodiscard]] const MeshResource& GetMesh() const noexcept;
	[[nodiscard]] TransformComponent& GetTransform() noexcept;
	[[nodiscard]] const TransformComponent& GetTransform() const noexcept;

  protected:
	void OnInitialize() override;
	void OnDestroy() noexcept override;

  private:
	// The resource owner must outlive the Scene that owns this component.
	MeshResource& Mesh;
	TransformComponent* Transform = nullptr;
};
