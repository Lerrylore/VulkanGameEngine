#include "RenderGraphSelfTest.h"

#include "RenderGraph.h"
#include "Renderer.h"

#include <algorithm>
#include <utility>

namespace
{
    void Check(
        RenderGraphSelfTest::Result& result,
        bool condition,
        std::string message)
    {
        if (!condition)
        {
            result.Failures.push_back(std::move(message));
        }
    }

    bool HasErrorContaining(
        const RenderGraph::CompilationResult& compilation,
        const std::string& text)
    {
        return std::any_of(
            compilation.Issues.begin(),
            compilation.Issues.end(),
            [&text](const RenderGraph::ValidationIssue& issue) {
                return issue.Level == RenderGraph::ValidationIssue::Severity::Error &&
                       issue.Message.find(text) != std::string::npos;
            });
    }

    bool HasPassNamed(
        const RenderGraph& graph,
        const RenderGraph::CompilationResult& compilation,
        const std::string& name)
    {
        return std::any_of(
            compilation.ExecutionOrder.begin(),
            compilation.ExecutionOrder.end(),
            [&graph, &name](const RenderGraph::PassId pass) {
                return graph.GetPassDescription(pass).Name == name;
            });
    }
}

RenderGraphSelfTest::Result RenderGraphSelfTest::Run()
{
    Result result;

    RenderGraph graph;
    const auto shadowMap = graph.AddResource(
        "ShadowMap",
        {.Format = vk::Format::eD32Sfloat,
         .Extent = {2048, 2048},
         .Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
         .InitialLayout = vk::ImageLayout::eUndefined,
         .FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal});
    const auto sceneColor = graph.AddResource(
        "SceneColor",
        {.Format = vk::Format::eR16G16B16A16Sfloat,
         .Extent = {1280, 720},
         .Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
         .InitialLayout = vk::ImageLayout::eUndefined,
         .FinalLayout = vk::ImageLayout::eTransferSrcOptimal});
    const auto swapchain = graph.AddResource(
        "Swapchain",
        {.Format = vk::Format::eR8G8B8A8Unorm,
         .Extent = {1280, 720},
         .Usage = vk::ImageUsageFlagBits::eTransferDst,
         .InitialLayout = vk::ImageLayout::eUndefined,
         .FinalLayout = vk::ImageLayout::ePresentSrcKHR,
         .Imported = true,
         .Transient = false});

    const auto shadow = graph.AddPass("Shadow", RenderGraph::PassType::Graphics);
    graph.Write(shadow, shadowMap, RenderGraph::ImageUsage::DepthAttachment);

    const auto forward = graph.AddPass("Forward", RenderGraph::PassType::Graphics);
    graph.Read(forward, shadowMap, RenderGraph::ImageUsage::Sampled);
    graph.Write(forward, sceneColor, RenderGraph::ImageUsage::ColorAttachment);

    const auto present = graph.AddPass("Present", RenderGraph::PassType::Transfer);
    graph.Read(present, sceneColor, RenderGraph::ImageUsage::TransferSource);
    graph.Write(present, swapchain, RenderGraph::ImageUsage::TransferDestination);

    const auto compilation = graph.Compile();
    Check(result, compilation.Succeeded, "basic graph should compile");
    Check(result, compilation.ExecutionOrder.size() == 3, "basic graph should contain three ordered passes");
    Check(result, compilation.ExecutionOrder[0] == shadow, "shadow should execute first");
    Check(result, compilation.ExecutionOrder[1] == forward, "forward should execute second");
    Check(result, compilation.ExecutionOrder[2] == present, "present should execute last");
    Check(result, compilation.Dependencies.size() == 2, "basic graph should contain two resource dependencies");

    const auto sceneLifetime = std::find_if(
        compilation.ResourceLifetimes.begin(),
        compilation.ResourceLifetimes.end(),
        [sceneColor](const RenderGraph::ResourceLifetime& lifetime) {
            return lifetime.Resource == sceneColor;
        });
    Check(result, sceneLifetime != compilation.ResourceLifetimes.end(), "scene color lifetime should exist");
    if (sceneLifetime != compilation.ResourceLifetimes.end())
    {
        Check(result, sceneLifetime->FirstPassOrder == 1, "scene color lifetime should start at forward");
        Check(result, sceneLifetime->LastPassOrder == 2, "scene color lifetime should end at present");
    }

    Check(result, graph.DumpText(compilation).find("Execution order") != std::string::npos, "text dump should describe execution order");
    Check(result, graph.DumpDot(compilation).find("digraph RenderGraph") != std::string::npos, "DOT dump should be valid Graphviz text");

    RenderGraph bufferGraph;
    const auto particleBuffer = bufferGraph.AddBuffer(
        "ParticleBuffer",
        {4096, true, false});
    const auto simulate = bufferGraph.AddPass("ParticleSimulation", RenderGraph::PassType::Compute);
    bufferGraph.Write(simulate, particleBuffer, RenderGraph::BufferUsage::Storage);
    const auto drawParticles = bufferGraph.AddPass("ParticleOverlay", RenderGraph::PassType::Graphics);
    bufferGraph.Read(drawParticles, particleBuffer, RenderGraph::BufferUsage::Vertex);

    const auto bufferCompilation = bufferGraph.Compile();
    Check(result, bufferCompilation.Succeeded, "compute-to-graphics buffer graph should compile");
    Check(result, bufferCompilation.ExecutionOrder.size() == 2, "buffer graph should contain two ordered passes");
    Check(result, bufferCompilation.ExecutionOrder[0] == simulate, "particle simulation should execute before particle overlay");
    Check(result, bufferCompilation.ExecutionOrder[1] == drawParticles, "particle overlay should execute after simulation");
    Check(result, bufferCompilation.Dependencies.size() == 1, "buffer graph should contain one dependency");
    Check(result, bufferCompilation.BufferLifetimes.size() == 1, "buffer graph should report one buffer lifetime");
    Check(result, bufferGraph.DumpText(bufferCompilation).find("Buffers:") != std::string::npos, "text dump should describe buffers");
    Check(result, bufferGraph.DumpDot(bufferCompilation).find("ParticleBuffer") != std::string::npos, "DOT dump should contain buffer resources");

    Renderer renderer;
    renderer.SetRenderPath(Renderer::RenderPath::Forward);
    const auto forwardFrame = renderer.BuildFrameGraph({
        .Extent = {1280, 720},
        .ShadowResolution = 2048,
        .MsaaSamples = vk::SampleCountFlagBits::e4,
        .ParticleBufferSize = 4096,
        .DepthPrepassEnabled = true,
        .ShadowFormat = vk::Format::eD32Sfloat,
        .SwapchainFormat = vk::Format::eR8G8B8A8Unorm,
        .ForwardColorFormat = vk::Format::eR8G8B8A8Unorm,
        .ForwardDepthFormat = vk::Format::eD32Sfloat});
    const auto forwardCompilation = forwardFrame.Graph.Compile();
    Check(result, forwardCompilation.Succeeded, "Renderer forward composition should compile");
    Check(result, HasPassNamed(forwardFrame.Graph, forwardCompilation, "ForwardOpaquePass"), "forward composition should contain ForwardOpaquePass");

    renderer.SetRenderPath(Renderer::RenderPath::Deferred);
    const auto deferredFrame = renderer.BuildFrameGraph({
        .Extent = {1280, 720},
        .ShadowResolution = 2048,
        .MsaaSamples = vk::SampleCountFlagBits::e1,
        .ParticleBufferSize = 4096,
        .DepthPrepassEnabled = true,
        .ShadowFormat = vk::Format::eD32Sfloat,
        .SwapchainFormat = vk::Format::eR8G8B8A8Unorm,
        .GBufferFormats = {
            vk::Format::eR8G8B8A8Unorm,
            vk::Format::eR16G16B16A16Sfloat,
            vk::Format::eR16G16B16A16Sfloat,
            vk::Format::eR16G16B16A16Sfloat},
        .GBufferDepthFormat = vk::Format::eD32Sfloat});
    const auto deferredCompilation = deferredFrame.Graph.Compile();
    Check(result, deferredCompilation.Succeeded, "Renderer deferred composition should compile");
    Check(result, HasPassNamed(deferredFrame.Graph, deferredCompilation, "GBufferPass"), "deferred composition should contain GBufferPass");
    Check(result, HasPassNamed(deferredFrame.Graph, deferredCompilation, "DeferredLightingPass"), "deferred composition should contain DeferredLightingPass");

    RenderGraph cycleGraph;
    const auto cyclePassA = cycleGraph.AddPass("CycleA", RenderGraph::PassType::Compute);
    const auto cyclePassB = cycleGraph.AddPass("CycleB", RenderGraph::PassType::Compute);
    cycleGraph.AddDependency(cyclePassA, cyclePassB);
    cycleGraph.AddDependency(cyclePassB, cyclePassA);
    const auto cycleCompilation = cycleGraph.Compile();
    Check(result, !cycleCompilation.Succeeded, "cyclic graph should fail compilation");
    Check(result, HasErrorContaining(cycleCompilation, "cycle detected"), "cycle error should be reported");

    RenderGraph lifetimeGraph;
    const auto uninitialized = lifetimeGraph.AddResource(
        "Uninitialized",
        {.Format = vk::Format::eR8G8B8A8Unorm,
         .Extent = {64, 64},
         .Usage = vk::ImageUsageFlagBits::eSampled,
         .InitialLayout = vk::ImageLayout::eUndefined,
         .FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal});
    const auto readPass = lifetimeGraph.AddPass("ReadBeforeWrite", RenderGraph::PassType::Graphics);
    lifetimeGraph.Read(readPass, uninitialized, RenderGraph::ImageUsage::Sampled);
    const auto lifetimeCompilation = lifetimeGraph.Compile();
    Check(result, !lifetimeCompilation.Succeeded, "read-before-write should fail validation");
    Check(result, HasErrorContaining(lifetimeCompilation, "read before its first write"), "lifetime error should be reported");

    result.Passed = result.Failures.empty();
    return result;
}

#ifdef RENDER_GRAPH_SELF_TEST_MAIN
#include <iostream>

int main()
{
    const auto result = RenderGraphSelfTest::Run();
    for (const auto& failure : result.Failures)
    {
        std::cerr << failure << '\n';
    }
    std::cout << (result.Passed ? "RenderGraph self-test passed\n" : "RenderGraph self-test failed\n");
    return result.Passed ? 0 : 1;
}
#endif
