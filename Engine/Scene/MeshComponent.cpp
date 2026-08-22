#include "MeshComponent.h"

#include "GameObject.h"
#include "TransformComponent.h"

#include "../Resources/MeshResource.h"

#include <cassert>
#include <stdexcept>

MeshComponent::MeshComponent(MeshResource& mesh) noexcept : Mesh(mesh)
{
}

MeshResource& MeshComponent::GetMesh() noexcept
{
	return Mesh;
}

const MeshResource& MeshComponent::GetMesh() const noexcept
{
	return Mesh;
}

TransformComponent& MeshComponent::GetTransform() noexcept
{
	assert(Transform != nullptr);
	return *Transform;
}

const TransformComponent& MeshComponent::GetTransform() const noexcept
{
	assert(Transform != nullptr);
	return *Transform;
}

void MeshComponent::OnInitialize()
{
	auto& transform = GetOwner().GetRequiredComponent<TransformComponent>();
	if (!transform.IsActive())
	{
		throw std::logic_error("MeshComponent requires an active TransformComponent");
	}
	Transform = &transform;
}

void MeshComponent::OnDestroy() noexcept
{
	Transform = nullptr;
}
