#include <algorithm>
#include <assert.h>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include <tiny_obj_loader.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include "Engine/Application/ApplicationLoop.h"
#include "Engine/Application/ApplicationConfig.h"
#include "Engine/Application/VulkanGameEngineApplication.h"
#include "Engine/Application/DebugViewController.h"
#include "Engine/Platform/Window.h"
#include "Engine/Renderer/FrameResources.h"
#include "Engine/Renderer/MeshRenderer.h"
#include "Engine/Renderer/RenderTargetResources.h"
#include "Engine/Renderer/ShadowMapResources.h"
#include "Engine/Renderer/SwapchainResources.h"
#include "Engine/Events/EventBus.h"
#include "Engine/Resources/BufferAllocation.h"
#include "Engine/Resources/BinaryFileLoader.h"
#include "Engine/Resources/MaterialResource.h"
#include "Engine/Resources/MeshResource.h"
#include "Engine/Resources/TextureResource.h"
#include "Engine/Resources/TextureFileLoader.h"
#include "Engine/Services/ServiceLocator.h"
#include "Engine/Scene/CameraComponent.h"
#include "Engine/Scene/DirectionalLightComponent.h"
#include "Engine/Scene/GameObject.h"
#include "Engine/Scene/MeshComponent.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Vulkan/VulkanContext.h"

#include <stb_image.h>
#include <stb_image_resize2.h>

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
struct Vertex
{
	glm::vec3 Position;
	glm::vec3 Color;
	glm::vec2 TexCoord;
	glm::vec3 Normal;
	glm::vec4 Tangent;

	static vk::VertexInputBindingDescription GetBindingDescription()
	{
		return { .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
	}

	static std::array<vk::VertexInputAttributeDescription, 5> GetAttributeDescriptions()
	{
		return { {{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, Position)},
				 {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, Color)},
				 {.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, TexCoord)},
				 {.location = 3, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, Normal)},
				 {.location = 4, .binding = 0, .format = vk::Format::eR32G32B32A32Sfloat, .offset = offsetof(Vertex, Tangent)}} };
	}

	bool operator==(const Vertex& other) const
	{
		return Position == other.Position && Color == other.Color && TexCoord == other.TexCoord && Normal == other.Normal;
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

		combine(vertex.Position.x);
		combine(vertex.Position.y);
		combine(vertex.Position.z);
		combine(vertex.Color.r);
		combine(vertex.Color.g);
		combine(vertex.Color.b);
		combine(vertex.TexCoord.x);
		combine(vertex.TexCoord.y);
		combine(vertex.Normal.x);
		combine(vertex.Normal.y);
		combine(vertex.Normal.z);
		return seed;
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
		Services.Register<EventBus>(Events);
		initVulkan();
		mainLoop();
	}

  private:
	// Declared first so it is destroyed last: the Vulkan surface must not outlive
	// the native window from which it was created.
	Window                           window{
		ApplicationConfig::WindowWidth,
		ApplicationConfig::WindowHeight,
		ApplicationConfig::WindowTitle.data()};
	ApplicationLoop                  Loop{window};
	VulkanContext                    vulkan{window, ApplicationConfig::EnableValidationLayers()};
	// Temporary non-owning aliases while rendering still lives in this class.
	// VulkanContext remains the sole owner of the handles.
	vk::raii::PhysicalDevice&        physicalDevice = vulkan.physicalDevice();
	vk::raii::Device&                device = vulkan.device();
	vk::raii::Queue&                 queue = vulkan.queue();
	SwapchainResources               swapchainResources{vulkan, window};
	// Temporary aliases until the renderer consumes SwapchainResources directly.
	vk::raii::SwapchainKHR&          swapChain = swapchainResources.handle();
	const std::vector<vk::Image>&    swapChainImages = swapchainResources.images();
	const vk::SurfaceFormatKHR&      swapChainSurfaceFormat = swapchainResources.surfaceFormat();
	const vk::Extent2D&              swapChainExtent = swapchainResources.extent();
	const std::vector<vk::raii::ImageView>& swapChainImageViews = swapchainResources.imageViews();
	RenderTargetResources            renderTargets{vulkan, swapchainResources};
	ShadowMapResources               shadowMapResources{vulkan, ApplicationConfig::MaxFramesInFlight};
	vk::raii::Image&                 depthImage = renderTargets.depthImage();
	vk::raii::ImageView&             depthImageView = renderTargets.depthImageView();
	vk::raii::Image&                 colorImage = renderTargets.colorImage();
	vk::raii::ImageView&             colorImageView = renderTargets.colorImageView();

	vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout pipelineLayout   = nullptr;
	vk::raii::Pipeline       graphicsPipeline = nullptr;
	vk::raii::PipelineLayout shadowPipelineLayout = nullptr;
	vk::raii::Pipeline       shadowGraphicsPipeline = nullptr;
	vk::raii::PipelineLayout particlePipelineLayout = nullptr;
	vk::raii::Pipeline       particleGraphicsPipeline = nullptr;

	vk::raii::DescriptorSetLayout computeDescriptorSetLayout = nullptr;
	vk::raii::PipelineLayout      computePipelineLayout = nullptr;
	vk::raii::Pipeline            computePipeline = nullptr;

	std::optional<MeshResource> meshResource;
	std::optional<TextureResource> textureResource;
	std::optional<TextureResource> normalMapResource;
	std::optional<TextureResource> metallicRoughnessMapResource;
	std::optional<MaterialResource> materialResource;

	std::vector<BufferAllocation> particleBuffers;
	std::vector<BufferAllocation> computeUniformBuffers;
	std::vector<void*>            computeUniformBuffersMapped;

	EventBus Events;
	ServiceLocator Services;
	Scene scene{Services};
	CameraComponent* ActiveCamera = nullptr;
	DirectionalLightComponent* DirectionalLight = nullptr;
	DebugViewMode CurrentDebugView = DebugViewMode::Lit;
	// Declared after Scene and its resources so it is destroyed before them.
	std::optional<MeshRenderer> MeshRendererInstance;
	vk::raii::DescriptorPool computeDescriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> computeDescriptorSets;

	FrameResources frameResources{vulkan, ApplicationConfig::MaxFramesInFlight, swapChainImages.size()};

	const vk::SampleCountFlagBits msaaSamples = vulkan.msaaSamples();
	std::chrono::steady_clock::time_point lastParticleUpdate = std::chrono::steady_clock::now();

	std::vector<Vertex>    vertices;
	std::vector<uint32_t>  indices;

	void initVulkan()
	{
		createDescriptorSetLayout();
		createComputeDescriptorSetLayout();
		createGraphicsPipeline();
		createShadowGraphicsPipeline();
		createParticleGraphicsPipeline();
		createComputePipeline();
		createTextureImage(
			textureResource,
			std::string(ApplicationConfig::BaseColorTexturePath),
			vk::Format::eR8G8B8A8Srgb);
		createTextureImage(
			normalMapResource,
			std::string(ApplicationConfig::NormalTexturePath),
			vk::Format::eR8G8B8A8Unorm);
		createTextureImage(
			metallicRoughnessMapResource,
			std::string(ApplicationConfig::MetallicRoughnessTexturePath),
			vk::Format::eR8G8B8A8Unorm);
		materialResource.emplace(*textureResource, *normalMapResource, *metallicRoughnessMapResource);
		loadModel();
		createGeometryBuffer();
		createParticleBuffers();
		setupGameObjects();
		MeshRendererInstance.emplace(
			vulkan,
			descriptorSetLayout,
			shadowMapResources,
			ApplicationConfig::MaxFramesInFlight);
		MeshRendererInstance->Build(scene);
		createComputeUniformBuffers();
		createComputeDescriptorPool();
		createComputeDescriptorSets();
		scene.Initialize();
		DebugViewController::PrintHelp();
	}

	void mainLoop()
	{
		Loop.Run([this](float deltaTime)
		{
			DebugViewController::Update(window, CurrentDebugView);
			scene.Update(deltaTime);
			drawFrame();
		});

		device.waitIdle();
		scene.Destroy();
		ActiveCamera = nullptr;
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
		assert(ActiveCamera != nullptr);
		ActiveCamera->SetAspectRatio(
			static_cast<float>(swapChainExtent.width) /
			static_cast<float>(swapChainExtent.height));
		renderTargets.recreate();
		frameResources.recreateSwapchainImages(swapChainImages.size());
		if (formatChanged)
		{
			createGraphicsPipeline();
			createParticleGraphicsPipeline();
		}
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
		vk::raii::ShaderModule shaderModule = createShaderModule(BinaryFileLoader::Load("Shaders/slang.spv"));

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"};
		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"};
		vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		auto                                     bindingDescription = Vertex::GetBindingDescription();
		auto                                     attributeDescriptions = Vertex::GetAttributeDescriptions();
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

	void createShadowGraphicsPipeline()
	{
		vk::raii::ShaderModule shaderModule = createShaderModule(BinaryFileLoader::Load("Shaders/slang.spv"));
		vk::PipelineShaderStageCreateInfo vertexShaderStageInfo{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = shaderModule,
			.pName = "shadowVertMain"};

		auto bindingDescription = Vertex::GetBindingDescription();
		auto attributeDescriptions = Vertex::GetAttributeDescriptions();
		const vk::VertexInputAttributeDescription positionAttribute = attributeDescriptions[0];
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &bindingDescription,
			.vertexAttributeDescriptionCount = 1,
			.pVertexAttributeDescriptions = &positionAttribute};
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};
		vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};
		vk::PipelineRasterizationStateCreateInfo rasterizer{
			.depthClampEnable = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = vk::CullModeFlagBits::eBack,
			.frontFace = vk::FrontFace::eCounterClockwise,
			.depthBiasEnable = vk::True,
			.depthBiasConstantFactor = 1.25f,
			.depthBiasSlopeFactor = 1.75f,
			.lineWidth = 1.0f};
		vk::PipelineMultisampleStateCreateInfo multisampling{
			.rasterizationSamples = vk::SampleCountFlagBits::e1};
		vk::PipelineDepthStencilStateCreateInfo depthStencil{
			.depthTestEnable = vk::True,
			.depthWriteEnable = vk::True,
			.depthCompareOp = vk::CompareOp::eLess,
			.depthBoundsTestEnable = vk::False,
			.stencilTestEnable = vk::False};
		std::array dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
		vk::PipelineDynamicStateCreateInfo dynamicState{
			.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
			.pDynamicStates = dynamicStates.data()};

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
			.setLayoutCount = 1,
			.pSetLayouts = &*descriptorSetLayout};
		shadowPipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineChain{
			{.stageCount = 1,
			 .pStages = &vertexShaderStageInfo,
			 .pVertexInputState = &vertexInputInfo,
			 .pInputAssemblyState = &inputAssembly,
			 .pViewportState = &viewportState,
			 .pRasterizationState = &rasterizer,
			 .pMultisampleState = &multisampling,
			 .pDepthStencilState = &depthStencil,
			 .pDynamicState = &dynamicState,
			 .layout = shadowPipelineLayout,
			 .renderPass = nullptr},
			{.colorAttachmentCount = 0,
			 .depthAttachmentFormat = shadowMapResources.GetFormat()}};
		shadowGraphicsPipeline = vk::raii::Pipeline(
			device,
			nullptr,
			pipelineChain.get<vk::GraphicsPipelineCreateInfo>());
	}

	void createParticleGraphicsPipeline()
	{
		vk::raii::ShaderModule shaderModule = createShaderModule(BinaryFileLoader::Load("Shaders/particles.spv"));
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
		vk::raii::ShaderModule shaderModule = createShaderModule(BinaryFileLoader::Load("Shaders/compute.spv"));
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

	void createTextureImage(
		std::optional<TextureResource>& destination,
		const std::string& texturePath,
		vk::Format textureFormat)
	{
		const TextureFile textureFile = TextureFileLoader::Load(texturePath);
		const int texWidth = textureFile.Width;
		const int texHeight = textureFile.Height;

		constexpr uint32_t bytesPerPixel = 4;
		const uint32_t mipLevels =
			static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

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
			memcpy(
				cpuMipPixels.data(),
				textureFile.Pixels.data(),
				static_cast<std::size_t>(cpuMipLevels.front().width) * cpuMipLevels.front().height * bytesPerPixel);

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
					throw std::runtime_error("failed to generate texture mipmaps in software!");
				}
			}
			stagingSize = totalSize;
		}

		BufferAllocation stagingBuffer{
			vulkan,
			stagingSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};

		void* data = stagingBuffer.memory().mapMemory(0, stagingSize);
		if (supportsLinearBlit)
		{
			memcpy(data, textureFile.Pixels.data(), static_cast<std::size_t>(stagingSize));
		}
		else
		{
			memcpy(data, cpuMipPixels.data(), static_cast<std::size_t>(stagingSize));
		}
		stagingBuffer.memory().unmapMemory();

		destination.emplace(
			vulkan,
			static_cast<uint32_t>(texWidth),
			static_cast<uint32_t>(texHeight),
			mipLevels,
			textureFormat);
		auto& image = destination->image();

		vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
		transitionImageLayout(commandBuffer, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, destination->mipLevels());
		if (supportsLinearBlit)
		{
			copyBufferToImage(commandBuffer, stagingBuffer.buffer(), image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
			generateMipmaps(commandBuffer, image, textureFormat, texWidth, texHeight, destination->mipLevels());
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
			commandBuffer.copyBufferToImage(*stagingBuffer.buffer(), image, vk::ImageLayout::eTransferDstOptimal, regions);
			transitionImageLayout(commandBuffer, image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, destination->mipLevels());
		}
		endSingleTimeCommands(std::move(commandBuffer));
	}

	#if 0 // Retained as a learning reference; external texture assets are now loaded above.
	void createProceduralNormalMapImage()
	{
		constexpr uint32_t width = 4;
		constexpr uint32_t height = 4;
		constexpr vk::Format normalMapFormat = vk::Format::eR8G8B8A8Unorm;
		constexpr std::array<uint8_t, width * height * 4> pixels{
			128, 128, 255, 255, 190, 128, 220, 255, 128, 128, 255, 255, 66, 128, 220, 255,
			128, 190, 220, 255, 128, 128, 255, 255, 128, 66, 220, 255, 128, 128, 255, 255,
			128, 128, 255, 255, 66, 128, 220, 255, 128, 128, 255, 255, 190, 128, 220, 255,
			128, 66, 220, 255, 128, 128, 255, 255, 128, 190, 220, 255, 128, 128, 255, 255};

		normalMapResource.emplace(vulkan, width, height, 1, normalMapFormat);
		BufferAllocation stagingBuffer{
			vulkan,
			sizeof(pixels),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
		void* mapped = stagingBuffer.memory().mapMemory(0, sizeof(pixels));
		std::memcpy(mapped, pixels.data(), sizeof(pixels));
		stagingBuffer.memory().unmapMemory();

		vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
		transitionImageLayout(
			commandBuffer,
			normalMapResource->image(),
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			1);
		const vk::BufferImageCopy region{
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1},
			.imageOffset = {0, 0, 0},
			.imageExtent = {width, height, 1}};
		commandBuffer.copyBufferToImage(
			*stagingBuffer.buffer(),
			*normalMapResource->image(),
			vk::ImageLayout::eTransferDstOptimal,
			region);
		transitionImageLayout(
			commandBuffer,
			normalMapResource->image(),
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			1);
		endSingleTimeCommands(std::move(commandBuffer));
	}

	void createProceduralMetallicRoughnessMapImage()
	{
		constexpr uint32_t width = 4;
		constexpr uint32_t height = 4;
		constexpr vk::Format materialMapFormat = vk::Format::eR8G8B8A8Unorm;
		// G stores roughness and B stores metallic. R and A are unused.
		constexpr std::array<uint8_t, width * height * 4> pixels{
			0, 32, 0, 255, 0, 96, 0, 255, 0, 160, 0, 255, 0, 224, 0, 255,
			0, 32, 128, 255, 0, 96, 128, 255, 0, 160, 128, 255, 0, 224, 128, 255,
			0, 32, 255, 255, 0, 96, 255, 255, 0, 160, 255, 255, 0, 224, 255, 255,
			0, 224, 255, 255, 0, 160, 255, 0, 32, 255, 255, 0, 96, 255, 255};

		metallicRoughnessMapResource.emplace(vulkan, width, height, 1, materialMapFormat);
		BufferAllocation stagingBuffer{
			vulkan,
			sizeof(pixels),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
		void* mapped = stagingBuffer.memory().mapMemory(0, sizeof(pixels));
		std::memcpy(mapped, pixels.data(), sizeof(pixels));
		stagingBuffer.memory().unmapMemory();

		vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
		transitionImageLayout(
			commandBuffer,
			metallicRoughnessMapResource->image(),
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			1);
		const vk::BufferImageCopy region{
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1},
			.imageOffset = {0, 0, 0},
			.imageExtent = {width, height, 1}};
		commandBuffer.copyBufferToImage(
			*stagingBuffer.buffer(),
			*metallicRoughnessMapResource->image(),
			vk::ImageLayout::eTransferDstOptimal,
			region);
		transitionImageLayout(
			commandBuffer,
			metallicRoughnessMapResource->image(),
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			1);
		endSingleTimeCommands(std::move(commandBuffer));
	}

	#endif

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
		vk::CommandBufferAllocateInfo allocInfo{ .commandPool = frameResources.commandPool(), .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
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
		std::array<vk::DescriptorSetLayoutBinding, 5> bindings{
			{{.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment},
			 {.binding = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment},
			 {.binding = 2, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment},
			 {.binding = 3, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment},
			 {.binding = 4, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}} };

		vk::DescriptorSetLayoutCreateInfo layoutInfo{ .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data() };

		descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
	}

	void loadModel()
	{
		const std::string modelPath(ApplicationConfig::ModelPath);
		tinyobj::attrib_t                attrib;
		std::vector<tinyobj::shape_t>    shapes;
		std::vector<tinyobj::material_t> materials;
		std::string                      warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str()))
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
					.Position = {
						attrib.vertices[positionOffset],
						attrib.vertices[positionOffset + 1],
						attrib.vertices[positionOffset + 2]},
					.Color = {1.0f, 1.0f, 1.0f},
					.TexCoord = {0.0f, 0.0f},
					.Normal = {0.0f, 0.0f, 1.0f},
					.Tangent = {1.0f, 0.0f, 0.0f, 1.0f}};

				if (index.normal_index < 0)
				{
					throw std::runtime_error("OBJ contains a face index without a vertex normal");
				}

				const std::size_t normalOffset = 3 * static_cast<std::size_t>(index.normal_index);
				if (normalOffset + 2 >= attrib.normals.size())
				{
					throw std::runtime_error("OBJ contains an invalid vertex normal index");
				}
				vertex.Normal = {
					attrib.normals[normalOffset],
					attrib.normals[normalOffset + 1],
					attrib.normals[normalOffset + 2]};

				if (index.texcoord_index >= 0)
				{
					const std::size_t texCoordOffset = 2 * static_cast<std::size_t>(index.texcoord_index);
					if (texCoordOffset + 1 >= attrib.texcoords.size())
					{
						throw std::runtime_error("OBJ contains an invalid texture-coordinate index");
					}

					vertex.TexCoord = {
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
			throw std::runtime_error("OBJ model contains no renderable geometry: " + modelPath);
		}

		std::vector<glm::vec3> tangentSums(vertices.size(), glm::vec3(0.0f));
		std::vector<glm::vec3> bitangentSums(vertices.size(), glm::vec3(0.0f));
		for (std::size_t index = 0; index + 2 < indices.size(); index += 3)
		{
			const Vertex& vertex0 = vertices[indices[index]];
			const Vertex& vertex1 = vertices[indices[index + 1]];
			const Vertex& vertex2 = vertices[indices[index + 2]];
			const glm::vec3 edge1 = vertex1.Position - vertex0.Position;
			const glm::vec3 edge2 = vertex2.Position - vertex0.Position;
			const glm::vec2 uvEdge1 = vertex1.TexCoord - vertex0.TexCoord;
			const glm::vec2 uvEdge2 = vertex2.TexCoord - vertex0.TexCoord;
			const float determinant = uvEdge1.x * uvEdge2.y - uvEdge1.y * uvEdge2.x;
			if (glm::abs(determinant) < 0.000001f)
			{
				continue;
			}

			const float inverseDeterminant = 1.0f / determinant;
			const glm::vec3 tangent = (edge1 * uvEdge2.y - edge2 * uvEdge1.y) * inverseDeterminant;
			const glm::vec3 bitangent = (edge2 * uvEdge1.x - edge1 * uvEdge2.x) * inverseDeterminant;
			tangentSums[indices[index]] += tangent;
			tangentSums[indices[index + 1]] += tangent;
			tangentSums[indices[index + 2]] += tangent;
			bitangentSums[indices[index]] += bitangent;
			bitangentSums[indices[index + 1]] += bitangent;
			bitangentSums[indices[index + 2]] += bitangent;
		}

		for (std::size_t index = 0; index < vertices.size(); ++index)
		{
			const glm::vec3 normal = glm::normalize(vertices[index].Normal);
			glm::vec3 tangent = tangentSums[index] - normal * glm::dot(normal, tangentSums[index]);
			if (glm::length(tangent) < 0.000001f)
			{
				const glm::vec3 reference = glm::abs(normal.z) < 0.9f
					? glm::vec3(0.0f, 0.0f, 1.0f)
					: glm::vec3(0.0f, 1.0f, 0.0f);
				tangent = glm::cross(reference, normal);
			}
			tangent = glm::normalize(tangent);
			const float handedness = glm::dot(glm::cross(normal, tangent), bitangentSums[index]) < 0.0f ? -1.0f : 1.0f;
			vertices[index].Tangent = glm::vec4(tangent, handedness);
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

		std::clog << "[OBJ] Model: " << modelPath << '\n'
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

		const vk::DeviceSize vertexBufferOffset = 0;
		const vk::DeviceSize indexBufferOffset =
			((vertexBufferSize + indexAlignment - 1) / indexAlignment) * indexAlignment;
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

		meshResource.emplace(
			vulkan,
			geometryBufferSize,
			vertexBufferOffset,
			indexBufferOffset,
			vk::IndexTypeValue<decltype(indices)::value_type>::value,
			static_cast<uint32_t>(indices.size()));

		copyBuffer(stagingBuffer.buffer(), meshResource->buffer(), geometryBufferSize);
	}

	void createParticleBuffers()
	{
		std::default_random_engine randomEngine(0xC0FFEEu);
		std::uniform_real_distribution<float> random01(0.0f, 1.0f);
		std::vector<Particle> particles(ApplicationConfig::ParticleCount);
		for (Particle& particle : particles)
		{
			const float radius = 0.25f * std::sqrt(random01(randomEngine));
			const float angle = random01(randomEngine) * 2.0f * glm::pi<float>();
			const float x = radius * std::cos(angle) *
				static_cast<float>(ApplicationConfig::WindowHeight) /
				static_cast<float>(ApplicationConfig::WindowWidth);
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
		particleBuffers.reserve(ApplicationConfig::MaxFramesInFlight);
		for (uint32_t frame = 0; frame < ApplicationConfig::MaxFramesInFlight; ++frame)
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
		assert(meshResource.has_value());
		assert(materialResource.has_value());
		materialResource->SetMetallic(1.0f);
		materialResource->SetRoughness(1.0f);
		materialResource->SetOcclusionStrength(1.0f);
		materialResource->SetEmissive({0.0f, 0.0f, 0.0f, 0.0f});

		auto& lightObject = scene.CreateGameObject();
		DirectionalLight = &lightObject.AddComponent<DirectionalLightComponent>();
		auto& directionalLight = *DirectionalLight;
		directionalLight.SetDirection({-0.55f, -0.7f, -1.0f});
		directionalLight.SetColor({1.0f, 0.93f, 0.82f});
		directionalLight.SetIntensity(1.0f);
		directionalLight.SetAmbientStrength(0.12f);

		auto& centerObject = scene.CreateGameObject();
		centerObject.GetTransform().SetPosition({0.0f, 0.0f, 0.0f});
		centerObject.GetTransform().SetRotation({0.0f, 0.0f, 0.0f});
		centerObject.GetTransform().SetScale({0.7f, 0.7f, 0.7f});
		centerObject.AddComponent<MeshComponent>(*meshResource, *materialResource);

		auto& leftObject = scene.CreateGameObject();
		leftObject.GetTransform().SetPosition({-1.35f, 0.0f, -0.35f});
		leftObject.GetTransform().SetRotation({0.0f, 0.0f, glm::radians(-25.0f)});
		leftObject.GetTransform().SetScale({0.55f, 0.55f, 0.55f});
		leftObject.AddComponent<MeshComponent>(*meshResource, *materialResource);

		auto& rightObject = scene.CreateGameObject();
		rightObject.GetTransform().SetPosition({1.35f, 0.0f, -0.35f});
		rightObject.GetTransform().SetRotation({0.0f, 0.0f, glm::radians(25.0f)});
		rightObject.GetTransform().SetScale({0.55f, 0.55f, 0.55f});
		rightObject.AddComponent<MeshComponent>(*meshResource, *materialResource);

		auto& cameraObject = scene.CreateGameObject();
		cameraObject.GetTransform().SetPosition({2.8f, 2.8f, 3.5f});
		ActiveCamera = &cameraObject.AddComponent<CameraComponent>();
		ActiveCamera->SetTarget({0.0f, 0.0f, 0.0f});
		ActiveCamera->SetUp({0.0f, 0.0f, 1.0f});
		ActiveCamera->SetFieldOfView(45.0f);
		ActiveCamera->SetAspectRatio(
			static_cast<float>(swapChainExtent.width) /
			static_cast<float>(swapChainExtent.height));
		ActiveCamera->SetClipPlanes(0.1f, 10.0f);
	}

	void createComputeUniformBuffers()
	{
		const vk::DeviceSize bufferSize = sizeof(ComputeUniformBufferObject);
		computeUniformBuffersMapped.clear();
		computeUniformBuffers.clear();
		computeUniformBuffers.reserve(ApplicationConfig::MaxFramesInFlight);
		computeUniformBuffersMapped.reserve(ApplicationConfig::MaxFramesInFlight);
		for (uint32_t frame = 0; frame < ApplicationConfig::MaxFramesInFlight; ++frame)
		{
			computeUniformBuffers.emplace_back(
				vulkan,
				bufferSize,
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			computeUniformBuffersMapped.emplace_back(
				computeUniformBuffers.back().memory().mapMemory(0, bufferSize));
		}
	}

	void createComputeDescriptorPool()
	{
		std::array<vk::DescriptorPoolSize, 2> poolSizes{{
			{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = ApplicationConfig::MaxFramesInFlight},
			{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = ApplicationConfig::MaxFramesInFlight * 2}}};
		vk::DescriptorPoolCreateInfo poolInfo{
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = ApplicationConfig::MaxFramesInFlight,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data()};
		computeDescriptorPool = vk::raii::DescriptorPool(device, poolInfo);
	}

	void createComputeDescriptorSets()
	{
		std::vector<vk::DescriptorSetLayout> layouts(ApplicationConfig::MaxFramesInFlight, *computeDescriptorSetLayout);
		vk::DescriptorSetAllocateInfo allocateInfo{
			.descriptorPool = computeDescriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
			.pSetLayouts = layouts.data()};
		computeDescriptorSets = device.allocateDescriptorSets(allocateInfo);

		const vk::DeviceSize particleBufferSize = sizeof(Particle) * ApplicationConfig::ParticleCount;
		for (uint32_t frame = 0; frame < ApplicationConfig::MaxFramesInFlight; ++frame)
		{
			const uint32_t previousFrame =
				(frame + ApplicationConfig::MaxFramesInFlight - 1) % ApplicationConfig::MaxFramesInFlight;
			vk::DescriptorBufferInfo uniformInfo{
				.buffer = *computeUniformBuffers[frame].buffer(),
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

	void recordComputeCommandBuffer()
	{
		const uint32_t frameIndex = frameResources.currentFrame();
		auto& commandBuffer = frameResources.computeCommandBuffer(frameIndex);
		commandBuffer.begin({});
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute,
			*computePipelineLayout,
			0,
			*computeDescriptorSets[frameIndex],
			{});
		commandBuffer.dispatch(
			(ApplicationConfig::ParticleCount + ApplicationConfig::ComputeWorkgroupSize - 1) /
				ApplicationConfig::ComputeWorkgroupSize,
			1,
			1);

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
		const uint32_t frameIndex = frameResources.currentFrame();
		auto &commandBuffer = frameResources.graphicsCommandBuffer(frameIndex);
		commandBuffer.begin({});
		recordShadowMapPass(commandBuffer, frameIndex);

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
		commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

		MeshRendererInstance->RecordDraws(
			commandBuffer,
			graphicsPipeline,
			pipelineLayout,
			frameIndex);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *particleGraphicsPipeline);
		commandBuffer.bindVertexBuffers(0, *particleBuffers[frameIndex].buffer(), {0});
		commandBuffer.draw(ApplicationConfig::ParticleCount, 1, 0, 0);
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

	void recordShadowMapPass(vk::raii::CommandBuffer& commandBuffer, uint32_t frameIndex)
	{
		const vk::raii::Image& shadowImage = shadowMapResources.GetImage(frameIndex);
		const vk::Extent2D shadowExtent{
			shadowMapResources.GetResolution(),
			shadowMapResources.GetResolution()};
		transition_image_layout(
			*shadowImage,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eTopOfPipe,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth);

		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
		vk::RenderingAttachmentInfo depthAttachmentInfo{
			.imageView = shadowMapResources.GetImageView(frameIndex),
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearDepth};
		vk::RenderingInfo renderingInfo{
			.renderArea = {.offset = {0, 0}, .extent = shadowExtent},
			.layerCount = 1,
			.colorAttachmentCount = 0,
			.pDepthAttachment = &depthAttachmentInfo};

		commandBuffer.beginRendering(renderingInfo);
		commandBuffer.setViewport(
			0,
			vk::Viewport(
				0.0f,
				0.0f,
				static_cast<float>(shadowExtent.width),
				static_cast<float>(shadowExtent.height),
				0.0f,
				1.0f));
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), shadowExtent));
		MeshRendererInstance->RecordShadowDraws(
			commandBuffer,
			shadowGraphicsPipeline,
			shadowPipelineLayout,
			frameIndex);
		commandBuffer.endRendering();

		transition_image_layout(
			*shadowImage,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eShaderSampledRead,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eFragmentShader,
			vk::ImageAspectFlagBits::eDepth);
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
		frameResources.graphicsCommandBuffer(frameResources.currentFrame()).pipelineBarrier2(dependency_info);
	}

	void drawFrame()
	{
		const uint32_t frameIndex = frameResources.currentFrame();
		// Per-frame resources use frameIndex; render-finished semaphores use the
		// independently acquired swapchain imageIndex.
		auto fenceResult = device.waitForFences(*frameResources.inFlightFence(frameIndex), vk::True, UINT64_MAX);
		if (fenceResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence!");
		}

		auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *frameResources.imageAvailableSemaphore(frameIndex), nullptr);

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
		device.resetFences(*frameResources.inFlightFence(frameIndex));

		updateUniformBuffer(frameIndex);
		updateComputeUniformBuffer(frameIndex);

		frameResources.computeCommandBuffer(frameIndex).reset();
		recordComputeCommandBuffer();
		const vk::SubmitInfo computeSubmitInfo{
			.commandBufferCount = 1,
			.pCommandBuffers = &*frameResources.computeCommandBuffer(frameIndex),
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*frameResources.computeFinishedSemaphore(frameIndex)};
		queue.submit(computeSubmitInfo, nullptr);

		frameResources.graphicsCommandBuffer(frameIndex).reset();
		recordCommandBuffer(imageIndex);

		std::array waitSemaphores{
			*frameResources.imageAvailableSemaphore(frameIndex),
			*frameResources.computeFinishedSemaphore(frameIndex)};
		std::array waitDestinationStageMasks{
			vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput),
			vk::PipelineStageFlags(vk::PipelineStageFlagBits::eVertexInput)};
		
		const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = static_cast<uint32_t>(waitSemaphores.size()),
		                                  .pWaitSemaphores      = waitSemaphores.data(),
		                                  .pWaitDstStageMask    = waitDestinationStageMasks.data(),
		                                  .commandBufferCount   = 1,
		                                  .pCommandBuffers      = &*frameResources.graphicsCommandBuffer(frameIndex),
		                                  .signalSemaphoreCount = 1,
		                                  .pSignalSemaphores    = &*frameResources.renderFinishedSemaphore(imageIndex)};
		queue.submit(submitInfo, *frameResources.inFlightFence(frameIndex));

		const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
			                                      .pWaitSemaphores    = &*frameResources.renderFinishedSemaphore(imageIndex),
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
		frameResources.advanceFrame();
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

		assert(ActiveCamera != nullptr && ActiveCamera->IsActive());
		const glm::mat4 view = ActiveCamera->GetViewMatrix();
		const glm::mat4 proj = ActiveCamera->GetProjectionMatrix();
		const glm::mat4 shadowViewProjection = GetShadowViewProjection();

		MeshRendererInstance->UpdateUniformBuffers(
			currentImage,
			view,
			proj,
			ActiveCamera->GetPosition(),
			shadowViewProjection,
			time,
			CurrentDebugView);
	}

	[[nodiscard]] glm::mat4 GetShadowViewProjection() const
	{
		assert(DirectionalLight != nullptr);
		const glm::vec3 lightDirection = DirectionalLight->GetDirection();
		const glm::vec3 lightPosition = -lightDirection * 8.0f;
		const glm::vec3 up = glm::abs(glm::dot(lightDirection, glm::vec3(0.0f, 0.0f, 1.0f))) > 0.95f
			? glm::vec3(0.0f, 1.0f, 0.0f)
			: glm::vec3(0.0f, 0.0f, 1.0f);
		const glm::mat4 lightView = glm::lookAt(
			lightPosition,
			glm::vec3(0.0f),
			up);
		const glm::mat4 lightProjection = glm::ortho(
			-5.0f,
			5.0f,
			-5.0f,
			5.0f,
			0.1f,
			20.0f);
		return lightProjection * lightView;
	}

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const
	{
		vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char), .pCode = reinterpret_cast<const uint32_t *>(code.data())};
		vk::raii::ShaderModule     shaderModule{device, createInfo};

		return shaderModule;
	}

};

int RunVulkanGameEngineApplication()
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
