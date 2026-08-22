#include "RenderComponent.h"

#include "GameObject.h"
#include "TransformComponent.h"

#include "../Resources/MeshResource.h"

#include <cassert>
#include <stdexcept>

RenderComponent::RenderComponent(MeshResource& mesh) noexcept : Mesh(mesh)
{
}

MeshResource& RenderComponent::GetMesh() noexcept
{
	return Mesh;
}

const MeshResource& RenderComponent::GetMesh() const noexcept
{
	return Mesh;
}

TransformComponent& RenderComponent::GetTransform() noexcept
{
	assert(Transform != nullptr);
	return *Transform;
}

const TransformComponent& RenderComponent::GetTransform() const noexcept
{
	assert(Transform != nullptr);
	return *Transform;
}

void RenderComponent::OnInitialize()
{
	auto& transform = GetOwner().GetRequiredComponent<TransformComponent>();
	if (!transform.IsActive())
	{
		throw std::logic_error("RenderComponent requires an active TransformComponent");
	}
	Transform = &transform;
}

void RenderComponent::OnDestroy() noexcept
{
	Transform = nullptr;
}
