#include "RenderGraphSelfTest.h"

#include "RenderGraph.h"

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
}

RenderGraphSelfTest::Result RenderGraphSelfTest::Run()
{
    Result result;

    RenderGraph graph;
    const auto shadowMap = graph.CreateImage(
        "ShadowMap",
        {{2048, 2048}, RenderGraph::ImageFormat::D32Sfloat, 1, 1, 1, false, true});
    const auto sceneColor = graph.CreateImage(
        "SceneColor",
        {{1280, 720}, RenderGraph::ImageFormat::R16G16B16A16Sfloat, 1, 1, 1, false, true});
    const auto swapchain = graph.CreateImage(
        "Swapchain",
        {{1280, 720}, RenderGraph::ImageFormat::R8G8B8A8Unorm, 1, 1, 1, true, false});

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

    RenderGraph cycleGraph;
    const auto cyclePassA = cycleGraph.AddPass("CycleA", RenderGraph::PassType::Compute);
    const auto cyclePassB = cycleGraph.AddPass("CycleB", RenderGraph::PassType::Compute);
    cycleGraph.AddDependency(cyclePassA, cyclePassB);
    cycleGraph.AddDependency(cyclePassB, cyclePassA);
    const auto cycleCompilation = cycleGraph.Compile();
    Check(result, !cycleCompilation.Succeeded, "cyclic graph should fail compilation");
    Check(result, HasErrorContaining(cycleCompilation, "cycle detected"), "cycle error should be reported");

    RenderGraph lifetimeGraph;
    const auto uninitialized = lifetimeGraph.CreateImage(
        "Uninitialized",
        {{64, 64}, RenderGraph::ImageFormat::R8G8B8A8Unorm, 1, 1, 1, false, true});
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
