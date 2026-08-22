#pragma once

#include "FrameResources.h"
#include "../Resources/BufferAllocation.h"
#include "../Vulkan/SingleTimeCommandExecutor.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

struct ParticleSystemConfig final
{
	uint32_t ParticleCount = 2048;
	uint32_t ComputeWorkgroupSize = 256;
	uint32_t MaxFramesInFlight = 2;
	float InitialAspectRatio = 0.75f;
};

class ParticleSystem final
{
  public:
	ParticleSystem(
		VulkanContext& vulkan,
		FrameResources& frameResources,
		SingleTimeCommandExecutor& singleTimeCommands,
		vk::Format colorFormat,
		vk::Format depthFormat,
		vk::SampleCountFlagBits msaaSamples,
		ParticleSystemConfig config = {});

	~ParticleSystem();

	ParticleSystem(const ParticleSystem&) = delete;
	ParticleSystem& operator=(const ParticleSystem&) = delete;
	ParticleSystem(ParticleSystem&&) = delete;
	ParticleSystem& operator=(ParticleSystem&&) = delete;

	void Initialize(
		const vk::raii::ShaderModule& particleShaderModule,
		const vk::raii::ShaderModule& computeShaderModule);

	// The workgroup size must match [numthreads(...)] in compute.slang.
	void Update(uint32_t frameIndex);
	void Update(uint32_t frameIndex, float deltaTime);

	void RecordComputeCommandBuffer(uint32_t frameIndex);
	void RecordDraw(vk::raii::CommandBuffer& commandBuffer, uint32_t frameIndex) const;

	void RebuildGraphicsPipeline(
		const vk::raii::ShaderModule& particleShaderModule,
		vk::Format colorFormat,
		vk::Format depthFormat,
		vk::SampleCountFlagBits msaaSamples);
	void RebuildComputePipeline(const vk::raii::ShaderModule& computeShaderModule);

	[[nodiscard]] uint32_t ParticleCount() const noexcept;
	[[nodiscard]] uint32_t ComputeWorkgroupSize() const noexcept;
	[[nodiscard]] uint32_t DispatchGroupCount() const noexcept;

  private:
	struct Particle
	{
		glm::vec2 position;
		glm::vec2 velocity;
		glm::vec4 color;

		static vk::VertexInputBindingDescription GetBindingDescription();
		static std::array<vk::VertexInputAttributeDescription, 2> GetAttributeDescriptions();
	};

	struct alignas(16) ComputeUniformBufferObject
	{
		float deltaTime = 0.0f;
	};

	void CreateParticleBuffers();
	void CreateComputeUniformBuffers();
	void CreateComputeDescriptorSetLayout();
	void CreateComputeDescriptorPool();
	void CreateComputeDescriptorSets();
	void CreateParticleGraphicsPipeline(const vk::raii::ShaderModule& particleShaderModule);
	void CreateComputePipeline(const vk::raii::ShaderModule& computeShaderModule);
	void ValidateInitialized() const;

	VulkanContext& Vulkan;
	FrameResources& FrameResourcesRef;
	SingleTimeCommandExecutor& SingleTimeCommands;
	ParticleSystemConfig Config;
	vk::Format ColorFormat;
	vk::Format DepthFormat;
	vk::SampleCountFlagBits MsaaSamples;

	// Declaration order is intentional: pipelines are destroyed before their layouts,
	// and descriptor sets are destroyed before their pool.
	vk::raii::DescriptorSetLayout ComputeDescriptorSetLayout = nullptr;
	vk::raii::PipelineLayout ParticlePipelineLayout = nullptr;
	vk::raii::Pipeline ParticleGraphicsPipeline = nullptr;
	vk::raii::PipelineLayout ComputePipelineLayout = nullptr;
	vk::raii::Pipeline ComputePipeline = nullptr;
	vk::raii::DescriptorPool ComputeDescriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> ComputeDescriptorSets;

	std::vector<BufferAllocation> ParticleBuffers;
	std::vector<BufferAllocation> ComputeUniformBuffers;
	std::vector<void*> ComputeUniformBuffersMapped;

	bool Initialized = false;
	std::chrono::steady_clock::time_point LastParticleUpdate = std::chrono::steady_clock::now();
};
