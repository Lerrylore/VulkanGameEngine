#include "ParticleSystem.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <glm/gtc/constants.hpp>
#include <random>
#include <stdexcept>

ParticleSystem::ParticleSystem(
	VulkanContext& vulkan,
	FrameResources& frameResources,
	SingleTimeCommandExecutor& singleTimeCommands,
	vk::Format colorFormat,
	vk::Format depthFormat,
	vk::SampleCountFlagBits msaaSamples,
	ParticleSystemConfig config)
	: Vulkan(vulkan),
	  FrameResourcesRef(frameResources),
	  SingleTimeCommands(singleTimeCommands),
	  Config(config),
	  ColorFormat(colorFormat),
	  DepthFormat(depthFormat),
	  MsaaSamples(msaaSamples)
{
	if (Config.ParticleCount == 0)
	{
		throw std::invalid_argument("particle count must be greater than zero");
	}
	if (Config.ComputeWorkgroupSize == 0)
	{
		throw std::invalid_argument("compute workgroup size must be greater than zero");
	}
	if (Config.MaxFramesInFlight == 0)
	{
		throw std::invalid_argument("frames in flight must be greater than zero");
	}
}

ParticleSystem::~ParticleSystem()
{
	for (std::size_t frame = 0; frame < ComputeUniformBuffersMapped.size(); ++frame)
	{
		ComputeUniformBuffers.at(frame).memory().unmapMemory();
	}
}

void ParticleSystem::Initialize(
	const vk::raii::ShaderModule& particleShaderModule,
	const vk::raii::ShaderModule& computeShaderModule)
{
	if (Initialized)
	{
		throw std::logic_error("particle system is already initialized");
	}

	CreateParticleBuffers();
	CreateComputeUniformBuffers();
	CreateComputeDescriptorSetLayout();
	CreateComputeDescriptorPool();
	CreateComputeDescriptorSets();
	CreateParticleGraphicsPipeline(particleShaderModule);
	CreateComputePipeline(computeShaderModule);
	Initialized = true;
}

void ParticleSystem::Update(uint32_t frameIndex)
{
	const auto now = std::chrono::steady_clock::now();
	const float deltaTime = std::clamp(
		std::chrono::duration<float>(now - LastParticleUpdate).count(),
		0.0f,
		0.05f);
	LastParticleUpdate = now;
	Update(frameIndex, deltaTime);
}

void ParticleSystem::Update(uint32_t frameIndex, float deltaTime)
{
	ValidateInitialized();
	if (frameIndex >= ComputeUniformBuffersMapped.size())
	{
		throw std::out_of_range("particle frame index is out of range");
	}

	const ComputeUniformBufferObject computeUbo{.deltaTime = deltaTime};
	std::memcpy(
		ComputeUniformBuffersMapped.at(frameIndex),
		&computeUbo,
		sizeof(computeUbo));
}

void ParticleSystem::RecordComputeCommandBuffer(uint32_t frameIndex)
{
	ValidateInitialized();
	if (frameIndex >= ComputeDescriptorSets.size())
	{
		throw std::out_of_range("particle frame index is out of range");
	}

	auto& commandBuffer = FrameResourcesRef.computeCommandBuffer(frameIndex);
	commandBuffer.begin({});
	RecordComputeCommands(commandBuffer, frameIndex);

	vk::BufferMemoryBarrier2 particleBarrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eVertexInput,
		.dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eVertexAttributeRead,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.buffer = *ParticleBuffers.at(frameIndex).buffer(),
		.offset = 0,
		.size = vk::WholeSize};
	vk::DependencyInfo particleDependency{
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers = &particleBarrier};
	commandBuffer.pipelineBarrier2(particleDependency);
	commandBuffer.end();
}

void ParticleSystem::RecordComputeCommands(
	vk::raii::CommandBuffer& commandBuffer,
	uint32_t frameIndex)
{
	ValidateInitialized();
	if (frameIndex >= ComputeDescriptorSets.size())
	{
		throw std::out_of_range("particle frame index is out of range");
	}

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *ComputePipeline);
	commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		*ComputePipelineLayout,
		0,
		*ComputeDescriptorSets.at(frameIndex),
		{});
	commandBuffer.dispatch(DispatchGroupCount(), 1, 1);
}

void ParticleSystem::RecordDraw(vk::raii::CommandBuffer& commandBuffer, uint32_t frameIndex) const
{
	ValidateInitialized();
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *ParticleGraphicsPipeline);
	commandBuffer.bindVertexBuffers(0, *ParticleBuffers.at(frameIndex).buffer(), {0});
	commandBuffer.draw(Config.ParticleCount, 1, 0, 0);
}

void ParticleSystem::RebuildGraphicsPipeline(
	const vk::raii::ShaderModule& particleShaderModule,
	vk::Format colorFormat,
	vk::Format depthFormat,
	vk::SampleCountFlagBits msaaSamples)
{
	ValidateInitialized();
	ColorFormat = colorFormat;
	DepthFormat = depthFormat;
	MsaaSamples = msaaSamples;
	CreateParticleGraphicsPipeline(particleShaderModule);
}

void ParticleSystem::RebuildComputePipeline(const vk::raii::ShaderModule& computeShaderModule)
{
	ValidateInitialized();
	CreateComputePipeline(computeShaderModule);
}

uint32_t ParticleSystem::ParticleCount() const noexcept
{
	return Config.ParticleCount;
}

uint32_t ParticleSystem::ComputeWorkgroupSize() const noexcept
{
	return Config.ComputeWorkgroupSize;
}

uint32_t ParticleSystem::DispatchGroupCount() const noexcept
{
	return (Config.ParticleCount + Config.ComputeWorkgroupSize - 1) /
		Config.ComputeWorkgroupSize;
}

vk::Buffer ParticleSystem::ParticleBuffer(uint32_t frameIndex) const
{
	ValidateInitialized();
	return *ParticleBuffers.at(frameIndex).buffer();
}

vk::DeviceSize ParticleSystem::ParticleBufferSize() const noexcept
{
	return sizeof(Particle) * Config.ParticleCount;
}

vk::VertexInputBindingDescription ParticleSystem::Particle::GetBindingDescription()
{
	return {.binding = 0, .stride = sizeof(Particle), .inputRate = vk::VertexInputRate::eVertex};
}

std::array<vk::VertexInputAttributeDescription, 2> ParticleSystem::Particle::GetAttributeDescriptions()
{
	return {{
		{.location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Particle, position)},
		{.location = 1, .binding = 0, .format = vk::Format::eR32G32B32A32Sfloat, .offset = offsetof(Particle, color)}}};
}

void ParticleSystem::CreateParticleBuffers()
{
	std::default_random_engine randomEngine(0xC0FFEEu);
	std::uniform_real_distribution<float> random01(0.0f, 1.0f);
	std::vector<Particle> particles(Config.ParticleCount);
	for (Particle& particle : particles)
	{
		const float radius = 0.25f * std::sqrt(random01(randomEngine));
		const float angle = random01(randomEngine) * 2.0f * glm::pi<float>();
		const float x = radius * std::cos(angle) * Config.InitialAspectRatio;
		const float y = radius * std::sin(angle);
		particle.position = {x, y};
		const glm::vec2 direction = glm::length(particle.position) > 0.0f
			? glm::normalize(particle.position)
			: glm::vec2(1.0f, 0.0f);
		particle.velocity = direction * 0.25f;
		particle.color = {random01(randomEngine), random01(randomEngine), random01(randomEngine), 1.0f};
	}

	const vk::DeviceSize bufferSize = sizeof(Particle) * particles.size();
	BufferAllocation stagingBuffer{
		Vulkan,
		bufferSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
	void* mapped = stagingBuffer.memory().mapMemory(0, bufferSize);
	std::memcpy(mapped, particles.data(), static_cast<std::size_t>(bufferSize));
	stagingBuffer.memory().unmapMemory();

	ParticleBuffers.clear();
	ParticleBuffers.reserve(Config.MaxFramesInFlight);
	for (uint32_t frame = 0; frame < Config.MaxFramesInFlight; ++frame)
	{
		ParticleBuffers.emplace_back(
			Vulkan,
			bufferSize,
			vk::BufferUsageFlagBits::eStorageBuffer |
				vk::BufferUsageFlagBits::eVertexBuffer |
				vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		SingleTimeCommands.CopyBuffer(stagingBuffer.buffer(), ParticleBuffers.back().buffer(), bufferSize);
	}
}

void ParticleSystem::CreateComputeUniformBuffers()
{
	const vk::DeviceSize bufferSize = sizeof(ComputeUniformBufferObject);
	ComputeUniformBuffers.clear();
	ComputeUniformBuffersMapped.clear();
	ComputeUniformBuffers.reserve(Config.MaxFramesInFlight);
	ComputeUniformBuffersMapped.reserve(Config.MaxFramesInFlight);
	for (uint32_t frame = 0; frame < Config.MaxFramesInFlight; ++frame)
	{
		ComputeUniformBuffers.emplace_back(
			Vulkan,
			bufferSize,
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		ComputeUniformBuffersMapped.emplace_back(
			ComputeUniformBuffers.back().memory().mapMemory(0, bufferSize));
	}
}

void ParticleSystem::CreateComputeDescriptorSetLayout()
{
	std::array<vk::DescriptorSetLayoutBinding, 3> bindings{{
		{.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute},
		{.binding = 1, .descriptorType = vk::DescriptorType::eStorageBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute},
		{.binding = 2, .descriptorType = vk::DescriptorType::eStorageBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute}}};
	vk::DescriptorSetLayoutCreateInfo layoutInfo{
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()};
	ComputeDescriptorSetLayout = vk::raii::DescriptorSetLayout(Vulkan.device(), layoutInfo);
}

void ParticleSystem::CreateComputeDescriptorPool()
{
	std::array<vk::DescriptorPoolSize, 2> poolSizes{{
		{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = Config.MaxFramesInFlight},
		{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = Config.MaxFramesInFlight * 2}}};
	vk::DescriptorPoolCreateInfo poolInfo{
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = Config.MaxFramesInFlight,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()};
	ComputeDescriptorPool = vk::raii::DescriptorPool(Vulkan.device(), poolInfo);
}

void ParticleSystem::CreateComputeDescriptorSets()
{
	std::vector<vk::DescriptorSetLayout> layouts(
		Config.MaxFramesInFlight,
		*ComputeDescriptorSetLayout);
	vk::DescriptorSetAllocateInfo allocateInfo{
		.descriptorPool = ComputeDescriptorPool,
		.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
		.pSetLayouts = layouts.data()};
	ComputeDescriptorSets = Vulkan.device().allocateDescriptorSets(allocateInfo);

	const vk::DeviceSize particleBufferSize = sizeof(Particle) * Config.ParticleCount;
	for (uint32_t frame = 0; frame < Config.MaxFramesInFlight; ++frame)
	{
		const uint32_t previousFrame =
			(frame + Config.MaxFramesInFlight - 1) % Config.MaxFramesInFlight;
		vk::DescriptorBufferInfo uniformInfo{
			.buffer = *ComputeUniformBuffers.at(frame).buffer(),
			.offset = 0,
			.range = sizeof(ComputeUniformBufferObject)};
		vk::DescriptorBufferInfo inputInfo{
			.buffer = *ParticleBuffers.at(previousFrame).buffer(),
			.offset = 0,
			.range = particleBufferSize};
		vk::DescriptorBufferInfo outputInfo{
			.buffer = *ParticleBuffers.at(frame).buffer(),
			.offset = 0,
			.range = particleBufferSize};

		std::array<vk::WriteDescriptorSet, 3> writes{{
			{.dstSet = ComputeDescriptorSets.at(frame), .dstBinding = 0, .descriptorCount = 1,
			 .descriptorType = vk::DescriptorType::eUniformBuffer, .pBufferInfo = &uniformInfo},
			{.dstSet = ComputeDescriptorSets.at(frame), .dstBinding = 1, .descriptorCount = 1,
			 .descriptorType = vk::DescriptorType::eStorageBuffer, .pBufferInfo = &inputInfo},
			{.dstSet = ComputeDescriptorSets.at(frame), .dstBinding = 2, .descriptorCount = 1,
			 .descriptorType = vk::DescriptorType::eStorageBuffer, .pBufferInfo = &outputInfo}}};
		Vulkan.device().updateDescriptorSets(writes, {});
	}
}

void ParticleSystem::CreateParticleGraphicsPipeline(const vk::raii::ShaderModule& particleShaderModule)
{
	std::array shaderStages{
		vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = particleShaderModule, .pName = "particleVertMain"},
		vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = particleShaderModule, .pName = "particleFragMain"}};

	const auto bindingDescription = Particle::GetBindingDescription();
	const auto attributeDescriptions = Particle::GetAttributeDescriptions();
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data()};
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::ePointList};
	vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};
	vk::PipelineRasterizationStateCreateInfo rasterizer{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eNone,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.lineWidth = 1.0f};
	vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples = MsaaSamples,
		.sampleShadingEnable = vk::True,
		.minSampleShading = 0.2f};
	vk::PipelineDepthStencilStateCreateInfo depthStencil{
		.depthTestEnable = vk::False,
		.depthWriteEnable = vk::False,
		.depthCompareOp = vk::CompareOp::eAlways};
	vk::PipelineColorBlendAttachmentState colorBlendAttachment{
		.blendEnable = vk::True,
		.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
		.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
		.colorBlendOp = vk::BlendOp::eAdd,
		.srcAlphaBlendFactor = vk::BlendFactor::eOne,
		.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
		.alphaBlendOp = vk::BlendOp::eAdd,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
	vk::PipelineColorBlendStateCreateInfo colorBlending{
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment};
	std::array dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
	vk::PipelineDynamicStateCreateInfo dynamicState{
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()};

	ParticlePipelineLayout = vk::raii::PipelineLayout(Vulkan.device(), vk::PipelineLayoutCreateInfo{});
	vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineChain{
		{.stageCount = static_cast<uint32_t>(shaderStages.size()),
		 .pStages = shaderStages.data(),
		 .pVertexInputState = &vertexInputInfo,
		 .pInputAssemblyState = &inputAssembly,
		 .pViewportState = &viewportState,
		 .pRasterizationState = &rasterizer,
		 .pMultisampleState = &multisampling,
		 .pDepthStencilState = &depthStencil,
		 .pColorBlendState = &colorBlending,
		 .pDynamicState = &dynamicState,
		 .layout = ParticlePipelineLayout,
		 .renderPass = nullptr},
		{.colorAttachmentCount = 1,
		 .pColorAttachmentFormats = &ColorFormat,
		 .depthAttachmentFormat = DepthFormat}};
	ParticleGraphicsPipeline = vk::raii::Pipeline(
		Vulkan.device(),
		nullptr,
		pipelineChain.get<vk::GraphicsPipelineCreateInfo>());
}

void ParticleSystem::CreateComputePipeline(const vk::raii::ShaderModule& computeShaderModule)
{
	vk::PipelineShaderStageCreateInfo shaderStage{
		.stage = vk::ShaderStageFlagBits::eCompute,
		.module = computeShaderModule,
		.pName = "compMain"};
	vk::PipelineLayoutCreateInfo layoutInfo{
		.setLayoutCount = 1,
		.pSetLayouts = &*ComputeDescriptorSetLayout};
	ComputePipelineLayout = vk::raii::PipelineLayout(Vulkan.device(), layoutInfo);
	vk::ComputePipelineCreateInfo pipelineInfo{
		.stage = shaderStage,
		.layout = ComputePipelineLayout};
	ComputePipeline = vk::raii::Pipeline(Vulkan.device(), nullptr, pipelineInfo);
}

void ParticleSystem::ValidateInitialized() const
{
	if (!Initialized)
	{
		throw std::logic_error("particle system has not been initialized");
	}
}
