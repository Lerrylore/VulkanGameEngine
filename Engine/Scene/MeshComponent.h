#pragma once

#include "Component.h"

class MeshResource;
class MaterialResource;
class TransformComponent;

class MeshComponent final : public Component
{
  public:
	MeshComponent(MeshResource& mesh, MaterialResource& material) noexcept;

	[[nodiscard]] MeshResource& GetMesh() noexcept;
	[[nodiscard]] const MeshResource& GetMesh() const noexcept;
	[[nodiscard]] MaterialResource& GetMaterial() noexcept;
	[[nodiscard]] const MaterialResource& GetMaterial() const noexcept;
	[[nodiscard]] TransformComponent& GetTransform() noexcept;
	[[nodiscard]] const TransformComponent& GetTransform() const noexcept;

  protected:
	void OnInitialize() override;
	void OnDestroy() noexcept override;

  private:
	// The resource owner must outlive the Scene that owns this component.
	MeshResource& Mesh;
	MaterialResource& Material;
	TransformComponent* Transform = nullptr;
};
