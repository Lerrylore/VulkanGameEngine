#pragma once

#include "../Vulkan/VulkanContext.h"

#include <glm/mat4x4.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

class Scene;
class DirectionalLightComponent;
class ShadowMapResources;

enum class DebugViewMode : uint32_t
{
	Lit = 0,
	WorldNormal = 1,
	TangentNormal = 2,
	Roughness = 3,
	Metallic = 4,
	Shadow = 5,
	Albedo = 6,
	GeometricNormal = 7,
	Tangent = 8,
	ShadowDepth = 9,
	Diffuse = 10,
	Specular = 11,
	Fresnel = 12,
	AmbientOcclusion = 13,
	Emissive = 14
};

class MeshRenderer final
{
  public:
	MeshRenderer(
		VulkanContext& vulkan,
		const vk::raii::DescriptorSetLayout& descriptorSetLayout,
		ShadowMapResources& shadowMaps,
		uint32_t framesInFlight);
	~MeshRenderer();

	MeshRenderer(const MeshRenderer&) = delete;
	MeshRenderer& operator=(const MeshRenderer&) = delete;
	MeshRenderer(MeshRenderer&&) = delete;
	MeshRenderer& operator=(MeshRenderer&&) = delete;

	// Build is intentionally one-shot and must happen before frame submission.
	// The Scene structure must remain static after this call until frame-safe
	// component change tracking is introduced.
	void Build(const Scene& scene);
	// The caller must wait for this frame's fence before writing its mapped UBOs.
	void UpdateUniformBuffers(
		uint32_t frameIndex,
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& cameraPosition,
		const glm::mat4& shadowViewProjection,
		float elapsedTime,
		DebugViewMode debugViewMode);
	void RecordShadowDraws(
		vk::raii::CommandBuffer& commandBuffer,
		const vk::raii::Pipeline& pipeline,
		const vk::raii::PipelineLayout& pipelineLayout,
		uint32_t frameIndex) const;
	void RecordDraws(
		vk::raii::CommandBuffer& commandBuffer,
		const vk::raii::Pipeline& pipeline,
		const vk::raii::PipelineLayout& pipelineLayout,
		uint32_t frameIndex) const;

	[[nodiscard]] std::size_t GetDrawCount() const noexcept;

  private:
	struct MeshDrawResources;

	void ValidateFrameIndex(uint32_t frameIndex) const;

	VulkanContext& Vulkan;
	const vk::raii::DescriptorSetLayout& DescriptorSetLayout;
	ShadowMapResources& ShadowMaps;
	uint32_t FrameCount = 0;

	// Draw resources contain the descriptor sets and are destroyed first.
	vk::raii::DescriptorPool DescriptorPool = nullptr;
	std::vector<MeshDrawResources> DrawResources;
	// Non-owning snapshot of the scene light. The Scene must outlive this renderer.
	const DirectionalLightComponent* DirectionalLight = nullptr;
	bool bBuilt = false;
};
