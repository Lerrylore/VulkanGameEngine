#include <algorithm>
#include <assert.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include "Engine/Platform/Window.h"
#include "Engine/Renderer/RenderTargetResources.h"
#include "Engine/Renderer/SwapchainResources.h"
#include "Engine/Resources/BufferAllocation.h"
#include "Engine/Resources/ImageAllocation.h"
#include "Engine/Vulkan/VulkanContext.h"

// STB Image implementation
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
constexpr uint32_t WIDTH                = 800;
constexpr uint32_t HEIGHT               = 600;
constexpr uint32_t PARTICLE_COUNT       = 8192;
constexpr uint32_t COMPUTE_WORKGROUP_SIZE = 256;
constexpr int      MAX_FRAMES_IN_FLIGHT = 2;
constexpr int      MAX_OBJECTS          = 3;
const std::string  MODEL_PATH = "Models/viking_room.obj";
const std::string  TEXTURE_PATH = "Textures/viking_room.png";

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return { .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
	}

	static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		return { {{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, pos)},
				 {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)},
				 {.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, texCoord)}} };
	}

	bool operator==(const Vertex& other) const
	{
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}
};

struct VertexHash
{
	std::size_t operator()(const Vertex& vertex) const noexcept
	{
		std::size_t seed = 0;
		auto combine = [&seed](float component)
		{
			seed ^= std::hash<float>{}(component) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		};

		combine(vertex.pos.x);
		combine(vertex.pos.y);
		combine(vertex.pos.z);
		combine(vertex.color.r);
		combine(vertex.color.g);
		combine(vertex.color.b);
		combine(vertex.texCoord.x);
		combine(vertex.texCoord.y);
		return seed;
	}
};

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct GameObject
{
	glm::vec3 position{0.0f};
	glm::vec3 rotation{0.0f};
	glm::vec3 scale{1.0f};

	// Memory is declared before the buffers so RAII destroys each buffer before
	// releasing the memory it is bound to.
	std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
	std::vector<vk::raii::Buffer>       uniformBuffers;
	std::vector<void *>                 uniformBuffersMapped;
	std::vector<vk::raii::DescriptorSet> descriptorSets;

	[[nodiscard]] glm::mat4 getModelMatrix() const
	{
		glm::mat4 model{1.0f};
		model = glm::translate(model, position);
		model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
		return glm::scale(model, scale);
	}
};

struct alignas(16) ComputeUniformBufferObject
{
	float deltaTime = 0.0f;
};

struct Particle
{
	glm::vec2 position;
	glm::vec2 velocity;
	glm::vec4 color;

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return {.binding = 0, .stride = sizeof(Particle), .inputRate = vk::VertexInputRate::eVertex};
	}

	static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
	{
		return {{
			{.location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Particle, position)},
			{.location = 1, .binding = 0, .format = vk::Format::eR32G32B32A32Sfloat, .offset = offsetof(Particle, color)}}};
	}
};

class VulkanGameEngineApplication
{
  public:
	void run()
	{
		initVulkan();
		mainLoop();
	}

  private:
	// Declared first so it is destroyed last: the Vulkan surface must not outlive
	// the native window from which it was created.
	Window                           window{WIDTH, HEIGHT, "Vulkan Game Engine"};
	VulkanContext                    vulkan{window, enableValidationLayers};
	// Temporary non-owning aliases while rendering still lives in this class.
	// VulkanContext remains the sole owner of the handles.
	vk::raii::PhysicalDevice&        physicalDevice = vulkan.physicalDevice();
	vk::raii::Device&                device = vulkan.device();
	const uint32_t                   queueIndex = vulkan.queueFamilyIndex();
	vk::raii::Queue&                 queue = vulkan.queue();
	SwapchainResources               swapchainResources{vulkan, window};
	// Temporary aliases until the renderer consumes SwapchainResources directly.
	vk::raii::SwapchainKHR&          swapChain = swapchainResources.handle();
	const std::vector<vk::Image>&    swapChainImages = swapchainResources.images();
	const vk::SurfaceFormatKHR&      swapChainSurfaceFormat = swapchainResources.surfaceFormat();
	const vk::Extent2D&              swapChainExtent = swapchainResources.extent();
	const std::vector<vk::raii::ImageView>& swapChainImageViews = swapchainResources.imageViews();
	RenderTargetResources            renderTargets{vulkan, swapchainResources};
	vk::raii::Image&                 depthImage = renderTargets.depthImage();
	vk::raii::ImageView&             depthImageView = renderTargets.depthImageView();
	vk::raii::Image&                 colorImage = renderTargets.colorImage();
	vk::raii::ImageView&             colorImageView = renderTargets.colorImageView();

	vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout pipelineLayout   = nullptr;
	vk::raii::Pipeline       graphicsPipeline = nullptr;
	vk::raii::PipelineLayout particlePipelineLayout = nullptr;
	vk::raii::Pipeline       particleGraphicsPipeline = nullptr;

	vk::raii::DescriptorSetLayout computeDescriptorSetLayout = nullptr;
	vk::raii::PipelineLayout      computePipelineLayout = nullptr;
	vk::raii::Pipeline            computePipeline = nullptr;

	std::optional<BufferAllocation> geometryBuffer;
	vk::DeviceSize         vertexBufferOffset   = 0;
	vk::DeviceSize         indexBufferOffset    = 0;

	std::vector<BufferAllocation> particleBuffers;
	std::vector<vk::raii::DeviceMemory> computeUniformBufferMemories;
	std::vector<vk::raii::Buffer>       computeUniformBuffers;
	std::vector<void*>                   computeUniformBuffersMapped;

	vk::raii::DescriptorPool descriptorPool = nullptr;
	std::array<GameObject, MAX_OBJECTS> gameObjects;
	vk::raii::DescriptorPool computeDescriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> computeDescriptorSets;

	uint32_t               mipLevels = 0;
	std::optional<ImageAllocation> textureImage;
	vk::raii::ImageView textureImageView = nullptr;
	vk::raii::Sampler      textureSampler = nullptr;

	vk::raii::CommandPool                commandPool = nullptr;
	std::vector<vk::raii::CommandBuffer> commandBuffers;
	std::vector<vk::raii::CommandBuffer> computeCommandBuffers;

	std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
	std::vector<vk::raii::Semaphore> computeFinishedSemaphores;
	std::vector<vk::raii::Fence>     inFlightFences;
	uint32_t                         frameIndex   = 0;

	const vk::SampleCountFlagBits msaaSamples = vulkan.msaaSamples();
	std::chrono::steady_clock::time_point lastParticleUpdate = std::chrono::steady_clock::now();

	std::vector<Vertex>    vertices;
	std::vector<uint32_t>  indices;

	void initVulkan()
	{
		createDescriptorSetLayout();
		createComputeDescriptorSetLayout();
		createGraphicsPipeline();
		createParticleGraphicsPipeline();
		createComputePipeline();
		createCommandPool();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		loadModel();
		createGeometryBuffer();
		createParticleBuffers();
		setupGameObjects();
		createUniformBuffers();
		createComputeUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createComputeDescriptorPool();
		createComputeDescriptorSets();
		createCommandBuffers();
		createSyncObjects();
	}

	void mainLoop()
	{
		while (!window.shouldClose())
		{
			window.pollEvents();
			drawFrame();
		}

		device.waitIdle();
	}

	void recreateSwapChain()
	{
		auto [width, height] = window.framebufferSize();
		while (width == 0 || height == 0)
		{
			window.waitEvents();
			std::tie(width, height) = window.framebufferSize();
		}

		device.waitIdle();

		const bool formatChanged = swapchainResources.recreate();
		renderTargets.recreate();
		renderFinishedSemaphores.clear();
		createRenderFinishedSemaphores();
		if (formatChanged)
		{
			createGraphicsPipeline();
			createParticleGraphicsPipeline();
		}
	}

	vk::raii::ImageView createImageView(vk::Image const& image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels)
	{
		vk::ImageViewCreateInfo viewInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = {.aspectMask = aspectFlags, .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = 1} };
		return vk::raii::ImageView(device, viewInfo);
	}

	void createComputeDescriptorSetLayout()
	{
		std::array<vk::DescriptorSetLayoutBinding, 3> bindings{{
			{.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute},
			{.binding = 1, .descriptorType = vk::DescriptorType::eStorageBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute},
			{.binding = 2, .descriptorType = vk::DescriptorType::eStorageBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute}}};
		vk::DescriptorSetLayoutCreateInfo layoutInfo{
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings = bindings.data()};
		computeDescriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
	}

	void createGraphicsPipeline()
	{
		vk::raii::ShaderModule shaderModule = createShaderModule(readFile("Shaders/slang.spv"));

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"};
		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"};
		vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		auto                                     bindingDescription = Vertex::getBindingDescription();
		auto                                     attributeDescriptions = Vertex::getAttributeDescriptions();
		vk::PipelineVertexInputStateCreateInfo   vertexInputInfo{ .vertexBindingDescriptionCount = 1,
																 .pVertexBindingDescriptions = &bindingDescription,
																 .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
																 .pVertexAttributeDescriptions = attributeDescriptions.data() };

		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};
		vk::PipelineViewportStateCreateInfo      viewportState{.viewportCount = 1, .scissorCount = 1};

		vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable = vk::False, .rasterizerDiscardEnable = vk::False, .polygonMode = vk::PolygonMode::eFill, .cullMode = vk::CullModeFlagBits::eBack, .frontFace = vk::FrontFace::eCounterClockwise, .depthBiasEnable = vk::False, .depthBiasSlopeFactor = 1.0f, .lineWidth = 1.0f};

		vk::PipelineMultisampleStateCreateInfo multisampling{
			.rasterizationSamples = msaaSamples,
			.sampleShadingEnable = vk::True,
			.minSampleShading = 0.2f};

		vk::PipelineColorBlendAttachmentState colorBlendAttachment{.blendEnable    = vk::False,
		                                                           .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

		vk::PipelineColorBlendStateCreateInfo colorBlending{.logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};

		std::vector dynamicStates = {
		    vk::DynamicState::eViewport,
		    vk::DynamicState::eScissor};
		vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};
		vk::PipelineDepthStencilStateCreateInfo depthStencil{
			.depthTestEnable = vk::True,
			.depthWriteEnable = vk::True,
			.depthCompareOp = vk::CompareOp::eLess,
			.depthBoundsTestEnable = vk::False,
			.stencilTestEnable = vk::False };

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
			.setLayoutCount = 1,
			.pSetLayouts = &*descriptorSetLayout};

		pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
		    {.stageCount          = 2,
		     .pStages             = shaderStages,
		     .pVertexInputState   = &vertexInputInfo,
		     .pInputAssemblyState = &inputAssembly,
		     .pViewportState      = &viewportState,
		     .pRasterizationState = &rasterizer,
		     .pMultisampleState   = &multisampling,
			 .pDepthStencilState  = &depthStencil,
		     .pColorBlendState    = &colorBlending,
		     .pDynamicState       = &dynamicState,
		     .layout              = pipelineLayout,
		     .renderPass          = nullptr},
		    {.colorAttachmentCount = 1,
			 .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
			 .depthAttachmentFormat = renderTargets.depthFormat()}};

		graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
	}

	void createParticleGraphicsPipeline()
	{
		vk::raii::ShaderModule shaderModule = createShaderModule(readFile("Shaders/particles.spv"));
		std::array shaderStages{
			vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "particleVertMain"},
			vk::PipelineShaderStageCreateInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "particleFragMain"}};

		auto bindingDescription = Particle::getBindingDescription();
		auto attributeDescriptions = Particle::getAttributeDescriptions();
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
			.rasterizationSamples = msaaSamples,
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

		particlePipelineLayout = vk::raii::PipelineLayout(device, vk::PipelineLayoutCreateInfo{});
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
			 .layout = particlePipelineLayout,
			 .renderPass = nullptr},
			{.colorAttachmentCount = 1,
			 .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
			 .depthAttachmentFormat = renderTargets.depthFormat()}};
		particleGraphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineChain.get<vk::GraphicsPipelineCreateInfo>());
	}

	void createComputePipeline()
	{
		vk::raii::ShaderModule shaderModule = createShaderModule(readFile("Shaders/compute.spv"));
		vk::PipelineShaderStageCreateInfo shaderStage{
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = shaderModule,
			.pName = "compMain"};
		vk::PipelineLayoutCreateInfo layoutInfo{
			.setLayoutCount = 1,
			.pSetLayouts = &*computeDescriptorSetLayout};
		computePipelineLayout = vk::raii::PipelineLayout(device, layoutInfo);
		vk::ComputePipelineCreateInfo pipelineInfo{.stage = shaderStage, .layout = computePipelineLayout};
		computePipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
	}

	void createCommandPool()
	{
		vk::CommandPoolCreateInfo poolInfo{.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		                                   .queueFamilyIndex = queueIndex};
		commandPool = vk::raii::CommandPool(device, poolInfo);
	}

	void createTextureImage()
	{
		int            texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pixels)
		{
			throw std::runtime_error("failed to load texture image!");
		}

		constexpr vk::Format textureFormat = vk::Format::eR8G8B8A8Srgb;
		constexpr uint32_t bytesPerPixel = 4;
		mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

		const vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(textureFormat);
		const vk::FormatFeatureFlags requiredBlitFeatures =
			vk::FormatFeatureFlagBits::eBlitSrc |
			vk::FormatFeatureFlagBits::eBlitDst |
			vk::FormatFeatureFlagBits::eSampledImageFilterLinear;
		const bool supportsLinearBlit =
			(formatProperties.optimalTilingFeatures & requiredBlitFeatures) == requiredBlitFeatures;

		struct CpuMipLevel
		{
			vk::DeviceSize offset;
			uint32_t width;
			uint32_t height;
		};

		std::vector<stbi_uc> cpuMipPixels;
		std::vector<CpuMipLevel> cpuMipLevels;
		vk::DeviceSize stagingSize = static_cast<vk::DeviceSize>(texWidth) * texHeight * bytesPerPixel;

		if (!supportsLinearBlit)
		{
			uint32_t mipWidth = static_cast<uint32_t>(texWidth);
			uint32_t mipHeight = static_cast<uint32_t>(texHeight);
			vk::DeviceSize totalSize = 0;
			for (uint32_t level = 0; level < mipLevels; ++level)
			{
				cpuMipLevels.push_back({totalSize, mipWidth, mipHeight});
				totalSize += static_cast<vk::DeviceSize>(mipWidth) * mipHeight * bytesPerPixel;
				mipWidth = std::max(1u, mipWidth / 2);
				mipHeight = std::max(1u, mipHeight / 2);
			}

			cpuMipPixels.resize(static_cast<std::size_t>(totalSize));
			memcpy(cpuMipPixels.data(), pixels, static_cast<std::size_t>(cpuMipLevels.front().width) * cpuMipLevels.front().height * bytesPerPixel);

			for (uint32_t level = 1; level < mipLevels; ++level)
			{
				const CpuMipLevel& source = cpuMipLevels[level - 1];
				const CpuMipLevel& destination = cpuMipLevels[level];
				if (!stbir_resize_uint8_srgb(
						cpuMipPixels.data() + source.offset,
						static_cast<int>(source.width), static_cast<int>(source.height), 0,
						cpuMipPixels.data() + destination.offset,
						static_cast<int>(destination.width), static_cast<int>(destination.height), 0,
						STBIR_RGBA))
				{
					stbi_image_free(pixels);
					throw std::runtime_error("failed to generate texture mipmaps in software!");
				}
			}
			stagingSize = totalSize;
		}

		auto [stagingBuffer, stagingBufferMemory] =
			createBuffer(stagingSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		void* data = stagingBufferMemory.mapMemory(0, stagingSize);
		if (supportsLinearBlit)
		{
			memcpy(data, pixels, static_cast<std::size_t>(stagingSize));
		}
		else
		{
			memcpy(data, cpuMipPixels.data(), static_cast<std::size_t>(stagingSize));
		}
		stagingBufferMemory.unmapMemory();

		stbi_image_free(pixels);

		textureImage.emplace(
			vulkan,
			static_cast<uint32_t>(texWidth),
			static_cast<uint32_t>(texHeight),
			mipLevels,
			vk::SampleCountFlagBits::e1,
			textureFormat,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferSrc |
				vk::ImageUsageFlagBits::eTransferDst |
				vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
		auto& image = textureImage->image();

		vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
		transitionImageLayout(commandBuffer, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);
		if (supportsLinearBlit)
		{
			copyBufferToImage(commandBuffer, stagingBuffer, image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
			generateMipmaps(commandBuffer, image, textureFormat, texWidth, texHeight, mipLevels);
		}
		else
		{
			std::vector<vk::BufferImageCopy> regions;
			regions.reserve(cpuMipLevels.size());
			for (uint32_t level = 0; level < mipLevels; ++level)
			{
				const CpuMipLevel& mip = cpuMipLevels[level];
				regions.push_back({
					.bufferOffset = mip.offset,
					.bufferRowLength = 0,
					.bufferImageHeight = 0,
					.imageSubresource = {
						.aspectMask = vk::ImageAspectFlagBits::eColor,
						.mipLevel = level,
						.baseArrayLayer = 0,
						.layerCount = 1},
					.imageOffset = {0, 0, 0},
					.imageExtent = {mip.width, mip.height, 1}});
			}
			commandBuffer.copyBufferToImage(stagingBuffer, image, vk::ImageLayout::eTransferDstOptimal, regions);
			transitionImageLayout(commandBuffer, image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, mipLevels);
		}
		endSingleTimeCommands(std::move(commandBuffer));
	}

	void generateMipmaps(vk::raii::CommandBuffer& commandBuffer,
		vk::raii::Image& image,
		vk::Format               imageFormat,
		int32_t                  texWidth,
		int32_t                  texHeight,
		uint32_t                 mipLevels)
	{
		// Check if image format supports linear blit-ing
		vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(imageFormat);

		if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
		{
			throw std::runtime_error("texture image format does not support linear blitting!");
		}

		vk::ImageMemoryBarrier barrier = { .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
										  .dstAccessMask = vk::AccessFlagBits::eTransferRead,
										  .oldLayout = vk::ImageLayout::eTransferDstOptimal,
										  .newLayout = vk::ImageLayout::eTransferSrcOptimal,
										  .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
										  .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
										  .image = image,
										  .subresourceRange = {
											  .aspectMask = vk::ImageAspectFlagBits::eColor,
											  .baseMipLevel = 0,
											  .levelCount = 1,
											  .baseArrayLayer = 0,
											  .layerCount = 1}
		};

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < mipLevels; i++)
		{
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

			commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

			vk::ImageBlit blit = { .srcSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = i - 1, .layerCount = 1},
					  .srcOffsets = std::array<vk::Offset3D, 2>({{}, {mipWidth, mipHeight, 1}}),
					  .dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = i, .layerCount = 1},
					  .dstOffsets = std::array<vk::Offset3D, 2>({{}, {1 < mipWidth ? mipWidth / 2 : 1, 1 < mipHeight ? mipHeight / 2 : 1, 1}}) };

			commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

			barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
			barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			commandBuffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eFragmentShader,
				{}, {}, {}, barrier);

			if (1 < mipWidth)
			{
				mipWidth /= 2;
			}
			if (1 < mipHeight)
			{
				mipHeight /= 2;
			}

		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		commandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader,
			{}, {}, {}, barrier);
	}

	void createTextureImageView()
	{
		textureImageView = createImageView(*textureImage->image(), vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, mipLevels);
	}

	void createTextureSampler()
	{
		vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
		vk::SamplerCreateInfo        samplerInfo{ .magFilter = vk::Filter::eLinear,
												 .minFilter = vk::Filter::eLinear,
												 .mipmapMode = vk::SamplerMipmapMode::eLinear,
												 .addressModeU = vk::SamplerAddressMode::eRepeat,
												 .addressModeV = vk::SamplerAddressMode::eRepeat,
												 .addressModeW = vk::SamplerAddressMode::eRepeat,
												 .anisotropyEnable = vk::True,
												 .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
												 .compareEnable = vk::False,
												 .compareOp = vk::CompareOp::eAlways };
		samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
		samplerInfo.unnormalizedCoordinates = vk::False;
		samplerInfo.compareEnable = vk::False;
		samplerInfo.compareOp = vk::CompareOp::eAlways;
		samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = static_cast<float>(mipLevels);

		textureSampler = vk::raii::Sampler(device, samplerInfo);
	}
	
	void transitionImageLayout(vk::raii::CommandBuffer& commandBuffer, const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels)
	{
		vk::PipelineStageFlags sourceStage;
		vk::PipelineStageFlags destinationStage;

		vk::ImageMemoryBarrier barrier{ .oldLayout = oldLayout,
							   .newLayout = newLayout,
							   .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
							   .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
							   .image = image,
							   .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = mipLevels, .layerCount = 1} };

		if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			sourceStage = vk::PipelineStageFlagBits::eTransfer;
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else
		{
			throw std::invalid_argument("unsupported layout transition!");
		}

		commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
	}

	void copyBufferToImage(vk::raii::CommandBuffer& commandBuffer, const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height)
	{
		vk::BufferImageCopy region{ .bufferOffset = 0,
						   .bufferRowLength = 0,
						   .bufferImageHeight = 0,
						   .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
						   .imageOffset = {0, 0, 0},
						   .imageExtent = {width, height, 1} };

		commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
	}

	vk::raii::CommandBuffer beginSingleTimeCommands()
	{
		vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
		vk::raii::CommandBuffer       commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());

		vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
		commandBuffer.begin(beginInfo);

		return std::move(commandBuffer);
	}

	void endSingleTimeCommands(vk::raii::CommandBuffer&& commandBuffer)
	{
		commandBuffer.end();

		vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandBuffer };
		queue.submit(submitInfo, nullptr);
		queue.waitIdle();
	}

	void createDescriptorSetLayout() 
	{
		std::array<vk::DescriptorSetLayoutBinding, 2> bindings{
			{{.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex},
			 {.binding = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}} };

		vk::DescriptorSetLayoutCreateInfo layoutInfo{ .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data() };

		descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
	}

	void loadModel()
	{
		tinyobj::attrib_t                attrib;
		std::vector<tinyobj::shape_t>    shapes;
		std::vector<tinyobj::material_t> materials;
		std::string                      warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
		{
			throw std::runtime_error(warn + err);
		}

		vertices.clear();
		indices.clear();

		std::size_t sourceIndexCount = 0;
		for (const auto& shape : shapes)
		{
			sourceIndexCount += shape.mesh.indices.size();
		}

		vertices.reserve(sourceIndexCount);
		indices.reserve(sourceIndexCount);
		std::unordered_map<Vertex, uint32_t, VertexHash> uniqueVertices;
		uniqueVertices.reserve(sourceIndexCount);

		for (const auto& shape : shapes)
		{
			for (const auto& index : shape.mesh.indices)
			{
				if (index.vertex_index < 0)
				{
					throw std::runtime_error("OBJ contains a face index without a vertex position");
				}

				const std::size_t positionOffset = 3 * static_cast<std::size_t>(index.vertex_index);
				if (positionOffset + 2 >= attrib.vertices.size())
				{
					throw std::runtime_error("OBJ contains an invalid vertex position index");
				}

				Vertex vertex{
					.pos = {
						attrib.vertices[positionOffset],
						attrib.vertices[positionOffset + 1],
						attrib.vertices[positionOffset + 2]},
					.color = {1.0f, 1.0f, 1.0f},
					.texCoord = {0.0f, 0.0f}};

				if (index.texcoord_index >= 0)
				{
					const std::size_t texCoordOffset = 2 * static_cast<std::size_t>(index.texcoord_index);
					if (texCoordOffset + 1 >= attrib.texcoords.size())
					{
						throw std::runtime_error("OBJ contains an invalid texture-coordinate index");
					}

					vertex.texCoord = {
						attrib.texcoords[texCoordOffset],
						1.0f - attrib.texcoords[texCoordOffset + 1]};
				}

				const auto [entry, inserted] = uniqueVertices.try_emplace(
					vertex, static_cast<uint32_t>(vertices.size()));
				if (inserted)
				{
					vertices.push_back(vertex);
				}
				indices.push_back(entry->second);
			}
		}

		if (vertices.empty() || indices.empty())
		{
			throw std::runtime_error("OBJ model contains no renderable geometry: " + MODEL_PATH);
		}

#ifndef NDEBUG
		const std::size_t expandedVertexCount = indices.size();
		const std::size_t uniqueVertexCount   = vertices.size();
		const std::size_t removedVertexCount  = expandedVertexCount - uniqueVertexCount;
		const double reductionPercentage =
			expandedVertexCount == 0
				? 0.0
				: 100.0 * static_cast<double>(removedVertexCount) / static_cast<double>(expandedVertexCount);
		const std::size_t bytesBeforeDeduplication = expandedVertexCount * sizeof(Vertex);
		const std::size_t bytesAfterDeduplication  = uniqueVertexCount * sizeof(Vertex);

		std::clog << "[OBJ] Model: " << MODEL_PATH << '\n'
			      << "[OBJ] Vertex references: " << expandedVertexCount << '\n'
			      << "[OBJ] Unique vertices: " << uniqueVertexCount << '\n'
			      << "[OBJ] Duplicates removed: " << removedVertexCount
			      << " (" << reductionPercentage << "%)\n"
			      << "[OBJ] Vertex memory: " << bytesBeforeDeduplication
			      << " -> " << bytesAfterDeduplication << " bytes\n";
#endif
	}

	void createGeometryBuffer()
	{
		const vk::DeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
		const vk::DeviceSize indexBufferSize  = sizeof(indices[0]) * indices.size();
		const vk::DeviceSize indexAlignment   = sizeof(indices[0]);

		vertexBufferOffset = 0;
		indexBufferOffset  = ((vertexBufferSize + indexAlignment - 1) / indexAlignment) * indexAlignment;
		const vk::DeviceSize geometryBufferSize = indexBufferOffset + indexBufferSize;

		BufferAllocation stagingBuffer{
			vulkan,
			geometryBufferSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};

		void* dataStaging = stagingBuffer.memory().mapMemory(0, geometryBufferSize);
		memset(dataStaging, 0, static_cast<size_t>(geometryBufferSize));
		memcpy(static_cast<char*>(dataStaging) + vertexBufferOffset, vertices.data(), static_cast<size_t>(vertexBufferSize));
		memcpy(static_cast<char*>(dataStaging) + indexBufferOffset, indices.data(), static_cast<size_t>(indexBufferSize));
		stagingBuffer.memory().unmapMemory();

		geometryBuffer.emplace(
			vulkan,
			geometryBufferSize,
			vk::BufferUsageFlagBits::eVertexBuffer |
				vk::BufferUsageFlagBits::eIndexBuffer |
				vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal);

		copyBuffer(stagingBuffer.buffer(), geometryBuffer->buffer(), geometryBufferSize);
	}

	void createParticleBuffers()
	{
		std::default_random_engine randomEngine(0xC0FFEEu);
		std::uniform_real_distribution<float> random01(0.0f, 1.0f);
		std::vector<Particle> particles(PARTICLE_COUNT);
		for (Particle& particle : particles)
		{
			const float radius = 0.25f * std::sqrt(random01(randomEngine));
			const float angle = random01(randomEngine) * 2.0f * glm::pi<float>();
			const float x = radius * std::cos(angle) * static_cast<float>(HEIGHT) / static_cast<float>(WIDTH);
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
			vulkan,
			bufferSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
		void* mapped = stagingBuffer.memory().mapMemory(0, bufferSize);
		memcpy(mapped, particles.data(), static_cast<std::size_t>(bufferSize));
		stagingBuffer.memory().unmapMemory();

		particleBuffers.clear();
		particleBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
		for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
		{
			particleBuffers.emplace_back(
				vulkan,
				bufferSize,
				vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
				vk::MemoryPropertyFlagBits::eDeviceLocal);
			copyBuffer(stagingBuffer.buffer(), particleBuffers.back().buffer(), bufferSize);
		}
	}

	void setupGameObjects()
	{
		gameObjects[0].position = {0.0f, 0.0f, 0.0f};
		gameObjects[0].rotation = {0.0f, 0.0f, 0.0f};
		gameObjects[0].scale    = {0.7f, 0.7f, 0.7f};

		gameObjects[1].position = {-1.35f, 0.0f, -0.35f};
		gameObjects[1].rotation = {0.0f, 0.0f, glm::radians(-25.0f)};
		gameObjects[1].scale    = {0.55f, 0.55f, 0.55f};

		gameObjects[2].position = {1.35f, 0.0f, -0.35f};
		gameObjects[2].rotation = {0.0f, 0.0f, glm::radians(25.0f)};
		gameObjects[2].scale    = {0.55f, 0.55f, 0.55f};
	}

	void createUniformBuffers()
	{
		for (auto &gameObject : gameObjects)
		{
			gameObject.uniformBuffersMemory.reserve(MAX_FRAMES_IN_FLIGHT);
			gameObject.uniformBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
			gameObject.uniformBuffersMapped.reserve(MAX_FRAMES_IN_FLIGHT);

			for (size_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
			{
				constexpr vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
				auto [buffer, bufferMemory] = createBuffer(
					bufferSize,
					vk::BufferUsageFlagBits::eUniformBuffer,
					vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
				gameObject.uniformBuffersMemory.emplace_back(std::move(bufferMemory));
				gameObject.uniformBuffers.emplace_back(std::move(buffer));
				gameObject.uniformBuffersMapped.emplace_back(
					gameObject.uniformBuffersMemory.back().mapMemory(0, bufferSize));
			}
		}
	}

	void createComputeUniformBuffers()
	{
		const vk::DeviceSize bufferSize = sizeof(ComputeUniformBufferObject);
		for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
		{
			auto [buffer, memory] = createBuffer(
				bufferSize,
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			computeUniformBufferMemories.emplace_back(std::move(memory));
			computeUniformBuffers.emplace_back(std::move(buffer));
			computeUniformBuffersMapped.emplace_back(computeUniformBufferMemories.back().mapMemory(0, bufferSize));
		}
	}

	void createDescriptorPool()
	{
		constexpr uint32_t descriptorCount = MAX_OBJECTS * MAX_FRAMES_IN_FLIGHT;
		std::array<vk::DescriptorPoolSize, 2> poolSize{ {{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = descriptorCount},
												{.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = descriptorCount}} };

		vk::DescriptorPoolCreateInfo          poolInfo{ .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
												   .maxSets = descriptorCount,
													   .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
													   .pPoolSizes = poolSize.data() };

		descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
	}

	void createDescriptorSets()
	{
		for (auto &gameObject : gameObjects)
		{
			std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
			vk::DescriptorSetAllocateInfo allocInfo{ .descriptorPool = descriptorPool,
													 .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
													 .pSetLayouts = layouts.data() };
			gameObject.descriptorSets = device.allocateDescriptorSets(allocInfo);

			for (size_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
			{
				vk::DescriptorBufferInfo bufferInfo{ .buffer = gameObject.uniformBuffers[frame], .offset = 0, .range = sizeof(UniformBufferObject) };
				vk::DescriptorImageInfo  imageInfo{ .sampler = textureSampler, .imageView = textureImageView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };

				std::array<vk::WriteDescriptorSet, 2> descriptorWrites{ {{.dstSet = gameObject.descriptorSets[frame],
																	 .dstBinding = 0,
																	 .dstArrayElement = 0,
																	 .descriptorCount = 1,
																	 .descriptorType = vk::DescriptorType::eUniformBuffer,
																	 .pBufferInfo = &bufferInfo},
																	{.dstSet = gameObject.descriptorSets[frame],
																	 .dstBinding = 1,
																	 .dstArrayElement = 0,
																	 .descriptorCount = 1,
																	 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
																	 .pImageInfo = &imageInfo}} };
				device.updateDescriptorSets(descriptorWrites, {});
			}
		}
	}

	void createComputeDescriptorPool()
	{
		std::array<vk::DescriptorPoolSize, 2> poolSizes{{
			{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = MAX_FRAMES_IN_FLIGHT},
			{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = MAX_FRAMES_IN_FLIGHT * 2}}};
		vk::DescriptorPoolCreateInfo poolInfo{
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = MAX_FRAMES_IN_FLIGHT,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data()};
		computeDescriptorPool = vk::raii::DescriptorPool(device, poolInfo);
	}

	void createComputeDescriptorSets()
	{
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *computeDescriptorSetLayout);
		vk::DescriptorSetAllocateInfo allocateInfo{
			.descriptorPool = computeDescriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
			.pSetLayouts = layouts.data()};
		computeDescriptorSets = device.allocateDescriptorSets(allocateInfo);

		const vk::DeviceSize particleBufferSize = sizeof(Particle) * PARTICLE_COUNT;
		for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
		{
			const uint32_t previousFrame = (frame + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT;
			vk::DescriptorBufferInfo uniformInfo{
				.buffer = computeUniformBuffers[frame],
				.offset = 0,
				.range = sizeof(ComputeUniformBufferObject)};
			vk::DescriptorBufferInfo inputInfo{
				.buffer = *particleBuffers[previousFrame].buffer(),
				.offset = 0,
				.range = particleBufferSize};
			vk::DescriptorBufferInfo outputInfo{
				.buffer = *particleBuffers[frame].buffer(),
				.offset = 0,
				.range = particleBufferSize};

			std::array<vk::WriteDescriptorSet, 3> writes{{
				{.dstSet = computeDescriptorSets[frame], .dstBinding = 0, .descriptorCount = 1,
				 .descriptorType = vk::DescriptorType::eUniformBuffer, .pBufferInfo = &uniformInfo},
				{.dstSet = computeDescriptorSets[frame], .dstBinding = 1, .descriptorCount = 1,
				 .descriptorType = vk::DescriptorType::eStorageBuffer, .pBufferInfo = &inputInfo},
				{.dstSet = computeDescriptorSets[frame], .dstBinding = 2, .descriptorCount = 1,
				 .descriptorType = vk::DescriptorType::eStorageBuffer, .pBufferInfo = &outputInfo}}};
			device.updateDescriptorSets(writes, {});
		}
	}

	void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size)
	{
		vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
		commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{ .size = size });
		endSingleTimeCommands(std::move(commandCopyBuffer));
	}

	std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
	{
		vk::BufferCreateInfo   bufferInfo{ .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive };
		vk::raii::Buffer       buffer = vk::raii::Buffer(device, bufferInfo);
		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size, .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties) };
		vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
		buffer.bindMemory(*bufferMemory, 0);
		return { std::move(buffer), std::move(bufferMemory) };
	}

	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) 
	{
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	void createCommandBuffers()
	{
		commandBuffers.clear();
		computeCommandBuffers.clear();
		vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
		commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
		computeCommandBuffers = vk::raii::CommandBuffers(device, allocInfo);
	}

	void recordComputeCommandBuffer()
	{
		auto& commandBuffer = computeCommandBuffers[frameIndex];
		commandBuffer.begin({});
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute,
			*computePipelineLayout,
			0,
			*computeDescriptorSets[frameIndex],
			{});
		commandBuffer.dispatch((PARTICLE_COUNT + COMPUTE_WORKGROUP_SIZE - 1) / COMPUTE_WORKGROUP_SIZE, 1, 1);

		vk::BufferMemoryBarrier2 particleBarrier{
			.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
			.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eVertexInput,
			.dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eVertexAttributeRead,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.buffer = *particleBuffers[frameIndex].buffer(),
			.offset = 0,
			.size = vk::WholeSize};
		vk::DependencyInfo particleDependency{
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers = &particleBarrier};
		commandBuffer.pipelineBarrier2(particleDependency);
		commandBuffer.end();
	}

	void recordCommandBuffer(uint32_t imageIndex)
	{

		auto &commandBuffer = commandBuffers[frameIndex];
		commandBuffer.begin({});

		// Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
		transition_image_layout(
			swapChainImages[imageIndex],
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::eColorAttachmentOptimal,
			{},                                                        // srcAccessMask
		    vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,		   // dstStage
			vk::ImageAspectFlagBits::eColor	
		);

		// Transition the multisampled color image to COLOR_ATTACHMENT_OPTIMAL
		transition_image_layout(
			*colorImage,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::ImageAspectFlagBits::eColor);

		// Transition depth image to depth attachment optimal layout
		transition_image_layout(
			*depthImage,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth);

		vk::ClearValue              clearColor     = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

		vk::RenderingAttachmentInfo attachmentInfo = {
		    .imageView          = colorImageView,
		    .imageLayout        = vk::ImageLayout::eColorAttachmentOptimal,
			.resolveMode        = vk::ResolveModeFlagBits::eAverage,
			.resolveImageView   = swapChainImageViews[imageIndex],
			.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		    .loadOp             = vk::AttachmentLoadOp::eClear,
		    .storeOp            = vk::AttachmentStoreOp::eStore,
		    .clearValue         = clearColor};

		vk::RenderingAttachmentInfo depthAttachmentInfo = {
			.imageView = depthImageView,
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = clearDepth };

		vk::RenderingInfo renderingInfo = {
		    .renderArea           = {.offset = {0, 0}, .extent = swapChainExtent},
		    .layerCount           = 1,
		    .colorAttachmentCount = 1,
		    .pColorAttachments    = &attachmentInfo,
			.pDepthAttachment = &depthAttachmentInfo};

		commandBuffer.beginRendering(renderingInfo);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
		commandBuffer.bindVertexBuffers(0, *geometryBuffer->buffer(), {vertexBufferOffset});
		commandBuffer.bindIndexBuffer(*geometryBuffer->buffer(), indexBufferOffset, vk::IndexTypeValue<decltype(indices)::value_type>::value);
		commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

		// Geometry, pipeline and texture are shared. The descriptor set selects the
		// transform uniform buffer belonging to the object drawn by this call.
		for (const auto &gameObject : gameObjects)
		{
			commandBuffer.bindDescriptorSets(
				vk::PipelineBindPoint::eGraphics,
				pipelineLayout,
				0,
				*gameObject.descriptorSets[frameIndex],
				nullptr);
			commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
		}

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *particleGraphicsPipeline);
		commandBuffer.bindVertexBuffers(0, *particleBuffers[frameIndex].buffer(), {0});
		commandBuffer.draw(PARTICLE_COUNT, 1, 0, 0);
		commandBuffer.endRendering();
		// After rendering, transition the swapchain image to PRESENT_SRC
		transition_image_layout(
		    swapChainImages[imageIndex],
		    vk::ImageLayout::eColorAttachmentOptimal,
		    vk::ImageLayout::ePresentSrcKHR,
		    vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
		    {},                                                        // dstAccessMask
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		    vk::PipelineStageFlagBits2::eBottomOfPipe,                 // dstStage
			vk::ImageAspectFlagBits::eColor
		);
		commandBuffer.end();
	}

	void transition_image_layout(
	    vk::Image               image,
	    vk::ImageLayout         old_layout,
	    vk::ImageLayout         new_layout,
	    vk::AccessFlags2        src_access_mask,
	    vk::AccessFlags2        dst_access_mask,
	    vk::PipelineStageFlags2 src_stage_mask,
	    vk::PipelineStageFlags2 dst_stage_mask,
		vk::ImageAspectFlags    image_aspect_flags)
	{
		vk::ImageMemoryBarrier2 barrier = {
		    .srcStageMask        = src_stage_mask,
		    .srcAccessMask       = src_access_mask,
		    .dstStageMask        = dst_stage_mask,
		    .dstAccessMask       = dst_access_mask,
		    .oldLayout           = old_layout,
		    .newLayout           = new_layout,
		    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .image               = image,
			.subresourceRange = {
				.aspectMask = image_aspect_flags,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1} };
		vk::DependencyInfo dependency_info = {
		    .dependencyFlags         = {},
		    .imageMemoryBarrierCount = 1,
		    .pImageMemoryBarriers    = &barrier};
		commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
	}

	void createSyncObjects()
	{
		assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && computeFinishedSemaphores.empty() && inFlightFences.empty());
		createRenderFinishedSemaphores();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			computeFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
		}
	}

	void createRenderFinishedSemaphores()
	{
		assert(renderFinishedSemaphores.empty());
		renderFinishedSemaphores.reserve(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); ++i)
		{
			renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		}
	}

	void drawFrame()
	{
		// Note: inFlightFences, presentCompleteSemaphores, and commandBuffers are indexed by frameIndex,
		//       while renderFinishedSemaphores is indexed by imageIndex
		auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
		if (fenceResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence!");
		}

		auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

		// Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
		// here and does not need to be caught by an exception.
		if (result == vk::Result::eErrorOutOfDateKHR)
		{
			recreateSwapChain();
			return;
		}
		// On other success codes than eSuccess and eSuboptimalKHR we just throw an exception.
		// On any error code, aquireNextImage already threw an exception.
		else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		{
			assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		// Only reset the fence if we are submitting work
		device.resetFences(*inFlightFences[frameIndex]);

		updateUniformBuffer(frameIndex);
		updateComputeUniformBuffer(frameIndex);

		computeCommandBuffers[frameIndex].reset();
		recordComputeCommandBuffer();
		const vk::SubmitInfo computeSubmitInfo{
			.commandBufferCount = 1,
			.pCommandBuffers = &*computeCommandBuffers[frameIndex],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*computeFinishedSemaphores[frameIndex]};
		queue.submit(computeSubmitInfo, nullptr);

		commandBuffers[frameIndex].reset();
		recordCommandBuffer(imageIndex);

		std::array waitSemaphores{
			*presentCompleteSemaphores[frameIndex],
			*computeFinishedSemaphores[frameIndex]};
		std::array waitDestinationStageMasks{
			vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput),
			vk::PipelineStageFlags(vk::PipelineStageFlagBits::eVertexInput)};
		
		const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = static_cast<uint32_t>(waitSemaphores.size()),
		                                  .pWaitSemaphores      = waitSemaphores.data(),
		                                  .pWaitDstStageMask    = waitDestinationStageMasks.data(),
		                                  .commandBufferCount   = 1,
		                                  .pCommandBuffers      = &*commandBuffers[frameIndex],
		                                  .signalSemaphoreCount = 1,
		                                  .pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex]};
		queue.submit(submitInfo, *inFlightFences[frameIndex]);

		const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
			                                      .pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex],
			                                      .swapchainCount     = 1,
			                                      .pSwapchains        = &*swapChain,
			                                      .pImageIndices      = &imageIndex};
		result = queue.presentKHR(presentInfoKHR);
		// Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
		// here and does not need to be caught by an exception.
		if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || window.wasFramebufferResized())
		{
			window.resetFramebufferResized();
			recreateSwapChain();
		}
		else
		{
			// There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
			assert(result == vk::Result::eSuccess);
		}
		frameIndex   = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void updateComputeUniformBuffer(uint32_t currentFrame)
	{
		const auto now = std::chrono::steady_clock::now();
		const float deltaTime = std::clamp(
			std::chrono::duration<float>(now - lastParticleUpdate).count(),
			0.0f,
			0.05f);
		lastParticleUpdate = now;
		const ComputeUniformBufferObject computeUbo{.deltaTime = deltaTime};
		memcpy(computeUniformBuffersMapped[currentFrame], &computeUbo, sizeof(computeUbo));
	}

	void updateUniformBuffer(uint32_t currentImage)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		const glm::mat4 view = lookAt(
			glm::vec3(2.8f, 2.8f, 3.5f),
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 proj = glm::perspective(
			glm::radians(45.0f),
			static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
			0.1f,
			10.0f);
		proj[1][1] *= -1;

		for (size_t objectIndex = 0; objectIndex < gameObjects.size(); ++objectIndex)
		{
			const auto &gameObject = gameObjects[objectIndex];
			const float direction = objectIndex % 2 == 0 ? 1.0f : -1.0f;

			UniformBufferObject ubo{};
			ubo.model = gameObject.getModelMatrix() * glm::rotate(
				glm::mat4(1.0f),
				direction * time * glm::radians(35.0f),
				glm::vec3(0.0f, 0.0f, 1.0f));
			ubo.view = view;
			ubo.proj = proj;

			memcpy(gameObject.uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
		}
	}

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const
	{
		vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char), .pCode = reinterpret_cast<const uint32_t *>(code.data())};
		vk::raii::ShaderModule     shaderModule{device, createInfo};

		return shaderModule;
	}

	static std::vector<char> readFile(const std::string &filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file!");
		}
		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		file.close();
		return buffer;
	}
};

int main()
{
	try
	{
		VulkanGameEngineApplication app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
