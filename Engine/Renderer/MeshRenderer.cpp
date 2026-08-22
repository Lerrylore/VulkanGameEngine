#include "MeshRenderer.h"

#include "../Resources/BufferAllocation.h"
#include "../Resources/MaterialResource.h"
#include "../Resources/MeshResource.h"
#include "../Resources/TextureResource.h"
#include "../Scene/MeshComponent.h"
#include "../Scene/DirectionalLightComponent.h"
#include "../Scene/Scene.h"
#include "../Scene/TransformComponent.h"
#include "ShadowMapResources.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
struct MeshUniformBufferObject
{
	glm::mat4 Model;
	glm::mat4 View;
	glm::mat4 Projection;
	glm::mat4 NormalMatrix;
	glm::mat4 ShadowViewProjection;
	glm::vec4 LightDirection;
	glm::vec4 LightColorIntensity;
	glm::vec4 MaterialBaseColorAmbient;
	glm::vec4 CameraPosition;
	glm::vec4 MaterialMetallicRoughness;
	glm::vec4 MaterialOcclusion;
	glm::vec4 MaterialEmissive;
	glm::vec4 DebugView;
	glm::vec4 ShadowMapParameters;
};
}

struct MeshRenderer::MeshDrawResources
{
	explicit MeshDrawResources(const MeshComponent& component) noexcept
		: Component(&component)
	{
	}

	~MeshDrawResources()
	{
		for (std::size_t index = 0; index < UniformBuffersMapped.size(); ++index)
		{
			UniformBuffers[index].memory().unmapMemory();
		}
	}

	MeshDrawResources(const MeshDrawResources&) = delete;
	MeshDrawResources& operator=(const MeshDrawResources&) = delete;

	MeshDrawResources(MeshDrawResources&& other) noexcept
		: Component(std::exchange(other.Component, nullptr))
	{
		UniformBuffers.swap(other.UniformBuffers);
		UniformBuffersMapped.swap(other.UniformBuffersMapped);
		DescriptorSets.swap(other.DescriptorSets);
	}

	MeshDrawResources& operator=(MeshDrawResources&&) = delete;

	const MeshComponent* Component = nullptr;
	std::vector<BufferAllocation> UniformBuffers;
	std::vector<void*> UniformBuffersMapped;
	std::vector<vk::raii::DescriptorSet> DescriptorSets;
};

MeshRenderer::MeshRenderer(
	VulkanContext& vulkan,
	const vk::raii::DescriptorSetLayout& descriptorSetLayout,
	ShadowMapResources& shadowMaps,
	uint32_t framesInFlight)
	: Vulkan(vulkan), DescriptorSetLayout(descriptorSetLayout), ShadowMaps(shadowMaps), FrameCount(framesInFlight)
{
	if (FrameCount == 0)
	{
		throw std::invalid_argument("MeshRenderer requires at least one frame in flight");
	}
}

MeshRenderer::~MeshRenderer() = default;

void MeshRenderer::Build(const Scene& scene)
{
	if (bBuilt)
	{
		throw std::logic_error("MeshRenderer cannot rebuild while its static Scene snapshot is in use");
	}

	std::vector<const MeshComponent*> components;
	scene.ForEachComponent<MeshComponent>([&components](const MeshComponent& component)
	{
		components.push_back(&component);
	});

	std::vector<const DirectionalLightComponent*> lights;
	scene.ForEachComponent<DirectionalLightComponent>([&lights](const DirectionalLightComponent& light)
	{
		lights.push_back(&light);
	});

	if (components.empty())
	{
		throw std::logic_error("MeshRenderer requires at least one MeshComponent");
	}
	if (lights.size() != 1)
	{
		throw std::logic_error("MeshRenderer requires exactly one DirectionalLightComponent");
	}
	if (components.size() > std::numeric_limits<uint32_t>::max() / FrameCount)
	{
		throw std::overflow_error("MeshRenderer descriptor count exceeds Vulkan limits");
	}

	const uint32_t descriptorCount = static_cast<uint32_t>(components.size()) * FrameCount;
	if (descriptorCount > std::numeric_limits<uint32_t>::max() / 4)
	{
		throw std::overflow_error("MeshRenderer sampled image descriptor count exceeds Vulkan limits");
	}
	const std::array<vk::DescriptorPoolSize, 2> poolSizes{{
		{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = descriptorCount},
		{.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = descriptorCount * 4}}};
	const vk::DescriptorPoolCreateInfo poolInfo{
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = descriptorCount,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()};
	DescriptorPool = vk::raii::DescriptorPool(Vulkan.device(), poolInfo);

	try
	{
		DrawResources.reserve(components.size());
		for (const MeshComponent* component : components)
		{
			DrawResources.emplace_back(*component);
			auto& drawResources = DrawResources.back();
			drawResources.UniformBuffers.reserve(FrameCount);
			drawResources.UniformBuffersMapped.reserve(FrameCount);

			for (uint32_t frameIndex = 0; frameIndex < FrameCount; ++frameIndex)
			{
				drawResources.UniformBuffers.emplace_back(
					Vulkan,
					sizeof(MeshUniformBufferObject),
					vk::BufferUsageFlagBits::eUniformBuffer,
					vk::MemoryPropertyFlagBits::eHostVisible |
						vk::MemoryPropertyFlagBits::eHostCoherent);
				void* mappedMemory = drawResources.UniformBuffers.back().memory().mapMemory(
					0,
					sizeof(MeshUniformBufferObject));
				drawResources.UniformBuffersMapped.push_back(mappedMemory);
			}

			const std::vector<vk::DescriptorSetLayout> layouts(FrameCount, *DescriptorSetLayout);
			const vk::DescriptorSetAllocateInfo allocateInfo{
				.descriptorPool = DescriptorPool,
				.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
				.pSetLayouts = layouts.data()};
			drawResources.DescriptorSets = Vulkan.device().allocateDescriptorSets(allocateInfo);

			const auto& baseColorTexture = component->GetMaterial().GetBaseColorTexture();
			const auto& normalTexture = component->GetMaterial().GetNormalTexture();
			const auto& metallicRoughnessTexture = component->GetMaterial().GetMetallicRoughnessTexture();
			for (uint32_t frameIndex = 0; frameIndex < FrameCount; ++frameIndex)
			{
				const vk::DescriptorBufferInfo bufferInfo{
					.buffer = *drawResources.UniformBuffers[frameIndex].buffer(),
					.offset = 0,
					.range = sizeof(MeshUniformBufferObject)};
				const vk::DescriptorImageInfo imageInfo{
					.sampler = *baseColorTexture.sampler(),
					.imageView = *baseColorTexture.imageView(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
				const vk::DescriptorImageInfo shadowMapInfo{
					.sampler = *ShadowMaps.GetSampler(frameIndex),
					.imageView = *ShadowMaps.GetImageView(frameIndex),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
				const vk::DescriptorImageInfo normalMapInfo{
					.sampler = *normalTexture.sampler(),
					.imageView = *normalTexture.imageView(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
				const vk::DescriptorImageInfo metallicRoughnessMapInfo{
					.sampler = *metallicRoughnessTexture.sampler(),
					.imageView = *metallicRoughnessTexture.imageView(),
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
				const std::array<vk::WriteDescriptorSet, 5> writes{{
					{.dstSet = drawResources.DescriptorSets[frameIndex],
					 .dstBinding = 0,
					 .descriptorCount = 1,
					 .descriptorType = vk::DescriptorType::eUniformBuffer,
					 .pBufferInfo = &bufferInfo},
					{.dstSet = drawResources.DescriptorSets[frameIndex],
					 .dstBinding = 1,
					 .descriptorCount = 1,
						 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
						 .pImageInfo = &imageInfo},
					{.dstSet = drawResources.DescriptorSets[frameIndex],
						 .dstBinding = 2,
						 .descriptorCount = 1,
						 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
						 .pImageInfo = &shadowMapInfo},
					{.dstSet = drawResources.DescriptorSets[frameIndex],
						 .dstBinding = 3,
						 .descriptorCount = 1,
						 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
						 .pImageInfo = &normalMapInfo},
					{.dstSet = drawResources.DescriptorSets[frameIndex],
						 .dstBinding = 4,
						 .descriptorCount = 1,
						 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
						 .pImageInfo = &metallicRoughnessMapInfo}}};
				Vulkan.device().updateDescriptorSets(writes, {});
			}
		}
		DirectionalLight = lights.front();
		bBuilt = true;
	}
	catch (...)
	{
		DrawResources.clear();
		DescriptorPool = nullptr;
		DirectionalLight = nullptr;
		throw;
	}
}

void MeshRenderer::UpdateUniformBuffers(
	uint32_t frameIndex,
	const glm::mat4& view,
	const glm::mat4& projection,
	const glm::vec3& cameraPosition,
	const glm::mat4& shadowViewProjection,
	float elapsedTime,
	DebugViewMode debugViewMode)
{
	ValidateFrameIndex(frameIndex);

	for (std::size_t drawIndex = 0; drawIndex < DrawResources.size(); ++drawIndex)
	{
		auto& drawResources = DrawResources[drawIndex];
		const auto& light = *DirectionalLight;
		const auto& material = drawResources.Component->GetMaterial();
		const float direction = drawIndex % 2 == 0 ? 1.0f : -1.0f;
		const glm::mat4 model = drawResources.Component->GetTransform().ModelMatrix() * glm::rotate(
				glm::mat4(1.0f),
				direction * elapsedTime * glm::radians(35.0f),
				glm::vec3(0.0f, 0.0f, 1.0f));
		const MeshUniformBufferObject uniformBuffer{
			.Model = model,
			.View = view,
			.Projection = projection,
			.NormalMatrix = glm::inverseTranspose(model),
			.ShadowViewProjection = shadowViewProjection,
			.LightDirection = glm::vec4(light.GetDirection(), 0.0f),
			.LightColorIntensity = glm::vec4(light.GetColor(), light.GetIntensity()),
			.MaterialBaseColorAmbient = glm::vec4(
				material.GetBaseColor().r,
				material.GetBaseColor().g,
				material.GetBaseColor().b,
				light.GetAmbientStrength()),
			.CameraPosition = glm::vec4(cameraPosition, 1.0f),
			.MaterialMetallicRoughness = glm::vec4(
				material.GetMetallic(),
				material.GetRoughness(),
				0.0f,
				0.0f),
			.MaterialOcclusion = glm::vec4(
				material.GetOcclusionStrength(),
				0.0f,
				0.0f,
				0.0f),
			.MaterialEmissive = material.GetEmissive(),
			.DebugView = glm::vec4(
				static_cast<float>(static_cast<uint32_t>(debugViewMode)),
				0.0f,
				0.0f,
				0.0f),
			.ShadowMapParameters = glm::vec4(
				1.0f / static_cast<float>(ShadowMaps.GetResolution()),
				1.0f / static_cast<float>(ShadowMaps.GetResolution()),
				0.001f,
				0.01f)};
		std::memcpy(
			drawResources.UniformBuffersMapped[frameIndex],
			&uniformBuffer,
			sizeof(uniformBuffer));
	}
}

void MeshRenderer::RecordShadowDraws(
	vk::raii::CommandBuffer& commandBuffer,
	const vk::raii::Pipeline& pipeline,
	const vk::raii::PipelineLayout& pipelineLayout,
	uint32_t frameIndex) const
{
	ValidateFrameIndex(frameIndex);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

	for (const auto& drawResources : DrawResources)
	{
		const auto& mesh = drawResources.Component->GetMesh();
		commandBuffer.bindVertexBuffers(0, *mesh.buffer(), {mesh.vertexOffset()});
		commandBuffer.bindIndexBuffer(*mesh.buffer(), mesh.indexOffset(), mesh.indexType());
		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			*pipelineLayout,
			0,
			*drawResources.DescriptorSets[frameIndex],
			nullptr);
		commandBuffer.drawIndexed(mesh.indexCount(), 1, 0, 0, 0);
	}
}

void MeshRenderer::RecordDraws(
	vk::raii::CommandBuffer& commandBuffer,
	const vk::raii::Pipeline& pipeline,
	const vk::raii::PipelineLayout& pipelineLayout,
	uint32_t frameIndex) const
{
	ValidateFrameIndex(frameIndex);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

	for (const auto& drawResources : DrawResources)
	{
		const auto& mesh = drawResources.Component->GetMesh();
		commandBuffer.bindVertexBuffers(0, *mesh.buffer(), {mesh.vertexOffset()});
		commandBuffer.bindIndexBuffer(*mesh.buffer(), mesh.indexOffset(), mesh.indexType());
		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			*pipelineLayout,
			0,
			*drawResources.DescriptorSets[frameIndex],
			nullptr);
		commandBuffer.drawIndexed(mesh.indexCount(), 1, 0, 0, 0);
	}
}

std::size_t MeshRenderer::GetDrawCount() const noexcept
{
	return DrawResources.size();
}

void MeshRenderer::ValidateFrameIndex(uint32_t frameIndex) const
{
	if (!bBuilt)
	{
		throw std::logic_error("MeshRenderer must be built before use");
	}
	if (DirectionalLight == nullptr)
	{
		throw std::logic_error("MeshRenderer has no directional light snapshot");
	}
	if (frameIndex >= FrameCount)
	{
		throw std::out_of_range("MeshRenderer frame index is out of range");
	}
}
