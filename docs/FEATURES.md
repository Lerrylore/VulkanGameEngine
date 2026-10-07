# Feature inventory

This inventory describes the code and distinguishes the published renderer from additions currently held in the local working tree. **Status: In progress.**

## Published renderer

| Feature | What it does | Source entry point |
| --- | --- | --- |
| Vulkan 1.4 initialization | Creates the instance/debug messenger/surface, checks device capabilities, enables required features, and creates the device/queues. | [VulkanContext](../Engine/Vulkan/VulkanContext.cpp) |
| RAII ownership | Uses `vk::raii` owners and explicit buffer/image allocation wrappers. | [BufferAllocation](../Engine/Resources/BufferAllocation.h), [ImageAllocation](../Engine/Resources/ImageAllocation.h) |
| Frame resources | Owns command and synchronization resources for two frames in flight. | [FrameResources](../Engine/Renderer/FrameResources.h) |
| Swapchain lifecycle | Selects presentation settings and rebuilds swapchain-dependent resources when needed. | [SwapchainResources](../Engine/Renderer/SwapchainResources.cpp) |
| Dynamic rendering | Records graphics work without a legacy `VkRenderPass` object. | [RenderPasses](../Engine/Renderer/RenderPasses.cpp) |
| Forward path | Renders scene meshes with PBR, shadows, MSAA, and particles. | [ForwardRenderer](../Engine/Renderer/ForwardRenderer.cpp), [forward shader](../Shaders/shader.slang) |
| Deferred path | Writes four G-buffer attachments, shades a fullscreen lighting pass, and composites particles. | [GBufferResources](../Engine/Renderer/GBufferResources.h), [G-buffer shader](../Shaders/gbuffer.slang), [lighting shader](../Shaders/deferred_lighting.slang) |
| Runtime path selection | Switches render paths and toggles the depth prepass. | [Renderer](../Engine/Renderer/Renderer.cpp), [application](../main_raii.cpp) |
| PBR | Implements GGX distribution, Smith geometry, Schlick Fresnel, and metallic/roughness diffuse/specular shading. | [forward shader](../Shaders/shader.slang), [deferred shader](../Shaders/deferred_lighting.slang) |
| Normal mapping | Applies tangent-space texture normals to material shading. | [forward shader](../Shaders/shader.slang), [G-buffer shader](../Shaders/gbuffer.slang) |
| Material data | Stores base color, normal and metallic/roughness textures, plus material factors and AO/emissive values. | [MaterialResource](../Engine/Resources/MaterialResource.h) |
| Directional shadows | Renders per-frame shadow maps and samples them with 3 × 3 PCF and angle-dependent bias. | [ShadowMapResources](../Engine/Renderer/ShadowMapResources.cpp), [forward shader](../Shaders/shader.slang) |
| Mesh rendering | Discovers mesh components in the scene and binds mesh/material GPU resources. | [MeshRenderer](../Engine/Renderer/MeshRenderer.cpp) |
| OBJ import and upload | Decodes mesh geometry with tinyobjloader and uploads vertex/index data. | [application](../main_raii.cpp), [MeshResource](../Engine/Resources/MeshResource.h) |
| Texture upload and sampling | Uses staging buffers, mipmap generation, a CPU mip fallback, and anisotropic samplers. | [application](../main_raii.cpp), [TextureResource](../Engine/Resources/TextureResource.cpp) |
| Compute particles | Dispatches a compute shader against particle storage buffers and renders the result. | [ParticleSystem](../Engine/Renderer/ParticleSystem.cpp), [compute shader](../Shaders/compute.slang), [particle shader](../Shaders/particles.slang) |
| Graph declarations | Registers typed images/buffers, access and usage declarations, and explicit pass dependencies. | [RenderGraph](../Engine/Renderer/RenderGraph.h) |
| Graph compilation | Compiles execution order, resource dependencies and lifetimes, and reports invalid graph configurations. | [RenderGraph](../Engine/Renderer/RenderGraph.cpp) |
| Graph execution | Binds physical resources and records synchronization2 image/buffer barriers and layout transitions. | [RenderGraphExecutor](../Engine/Renderer/RenderGraphExecutor.cpp) |
| Graph visualization | Exports text and DOT; the PowerShell tool converts DOT to SVG with Graphviz. | [RenderGraph](../Engine/Renderer/RenderGraph.cpp), [visualization tool](../Tools/RenderGraph.ps1) |
| Cached resources | Uses type + resource ID keys, typed handles, caching, reference counting, reload, and release operations. | [ResourceManager](../Engine/Resources/ResourceManager.h) |
| Background loading | Queues resource factories and callbacks on a worker thread. | [AsyncResourceManager](../Engine/Resources/AsyncResourceManager.h) |
| Shader hot reload | Polls watched file modification times, reloads resources, and invokes dependent pipeline callbacks. | [HotReloadResourceManager](../Engine/Resources/HotReloadResourceManager.h), [application](../main_raii.cpp) |
| File-chunk streaming | Queues prioritized chunk reads and processes a bounded number of requests on the calling thread. | [ResourceStreamingManager](../Engine/Resources/ResourceStreamingManager.h) |
| Scene and components | Provides scene-owned game objects, component access/lifecycle, transforms, cameras, meshes, and directional lights. | [Scene](../Engine/Scene/Scene.h), [GameObject](../Engine/Scene/GameObject.h) |
| Typed events | Dispatches typed scene events and manages subscription lifetimes. | [EventBus](../Engine/Events/EventBus.h), [EventSubscription](../Engine/Events/EventSubscription.h) |
| Engine services | Registers and exposes shared engine services. | [ServiceLocator](../Engine/Services/ServiceLocator.h) |
| Material diagnostics | Selects 15 views for normals, tangents, material channels, shadows, and lighting terms. | [DebugViewController](../Engine/Application/DebugViewController.cpp) |
| Existing self-tests | Exercises resource management and render-graph behavior. Debug startup invokes the self-tests. | [resource tests](../Engine/Resources/ResourceManagerSelfTest.cpp), [graph tests](../Engine/Renderer/RenderGraphSelfTest.cpp), [application](../main_raii.cpp) |
| Shader build integration | Compiles all active Slang graphics/compute entry points to SPIR-V as a Visual Studio build step. | [compile script](../Shaders/compile.bat), [project](../VulkanGameEngine.vcxproj) |

## Latest local additions

These files and changes are pending source publication; paths below describe the working tree rather than the published revision.

| Feature | Current implementation | Local source |
| --- | --- | --- |
| Free camera | Delta-time movement, yaw/pitch, pitch clamping, and sprint. | `Engine/Application/FreeCameraController.cpp` |
| Orbit particles | A 48-byte particle layout, world-space orbit simulation, view-projection push constants, and scene-depth occlusion in both paths. | `Engine/Renderer/ParticleSystem.cpp`, `Shaders/compute.slang`, `Shaders/particles.slang`, `Engine/Renderer/RenderPasses.cpp` |
| CPU profiler | Nested update/frame sections and per-pass command-recording times, aggregated by renderer/prepass configuration. | `Engine/Renderer/FrameProfiler.cpp` |
| GPU profiler | Query-pool timestamp measurements for frames and individual passes when supported by the selected queue. | `Engine/Renderer/FrameProfiler.cpp` |
| Smoke-test matrix | Five cases covering Debug/Release, Forward/Deferred, prepass states, and selected debug views. | `Tools/SmokeTest.ps1` |
| Report history | HTML overview, separate renderer pages, CPU/GPU breakdowns, sortable timing views, and JSON run history. | `Tools/SmokeTest.ps1` |
| Presentation override | `VPT_PRESENT_MODE` selects FIFO, Mailbox, or Immediate when supported. | `Engine/Renderer/SwapchainResources.cpp` |
| Reproducible startup | Environment settings choose render path, prepass, debug view, and a finite smoke-test frame count. | `main_raii.cpp` |
| Platform event dispatch | Window/input integration and platform-event declarations. | `Engine/Platform/WindowEventDispatcher.cpp`, `Engine/Events/PlatformEvents.h` |
| Lifecycle checks | Additional event-bus tests and resource-handle manager-lifetime handling. | `Engine/Events/EventBusSelfTest.cpp`, `Engine/Resources/ResourceManager.h` |
| Asset learning path | Box, DamagedHelmet, and Lantern sample exercises; GLB import remains a planned implementation. | `Assets/Samples/README.md`, `Exercises.md` |

## Scope and next steps

The current engine has a rasterization pipeline and compute particle simulation. It does not yet provide ray tracing/path tracing, a glTF runtime importer, point-light parity, clustered lighting, screen-space AO, bloom, or automatic graph memory aliasing.

The short roadmap is maintained in the [main README](../README.md#small-roadmap). Feature presence above is grounded in source inspection; this documentation update does not claim a fresh build, validation run, performance result, or clean-machine setup test.
