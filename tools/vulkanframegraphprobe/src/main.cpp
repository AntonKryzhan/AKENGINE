#include <AK/Render/VulkanFrameGraphCompiler.hpp>

#include <iostream>

int main()
{
    const AK::VulkanFrameGraphCompilerProbe probe = AK::BuildVulkanFrameGraphCompilerProbe();
    const AK::VulkanCompiledFrameGraph& graph = probe.compiled;

    std::cout << (probe.ok ? "[ ok ] " : "[fail] ")
              << "vulkan framegraph compiler / sync2 transient aliasing foundation"
              << " passes=" << graph.passes.size()
              << " resources=" << probe.graph.resources.size()
              << " barriers=" << graph.barriers.size()
              << " alias_groups=" << graph.aliasGroups.size()
              << " batches=" << graph.queueBatches.size()
              << " saved=" << graph.transientBytesSaved
              << " graphics=" << graph.graphicsPasses
              << " transfer=" << graph.transferPasses
              << " compute=" << graph.computePasses
              << " present=" << graph.presentPasses
              << " secondary=" << graph.secondaryCommandBuffers
              << " warnings=" << graph.validation.warnings
              << " ready=" << (graph.readyForRenderGraphExecution ? "true" : "false")
              << '\n';

    std::cout << AK::ToDebugString(graph) << '\n';
    std::cout << AK::ToDebugString(graph.validation) << '\n';
    if (!probe.graph.resources.empty())
    {
        std::cout << AK::ToDebugString(probe.graph.resources.front()) << '\n';
    }
    if (!probe.graph.passes.empty())
    {
        std::cout << AK::ToDebugString(probe.graph.passes.front()) << '\n';
    }
    if (!graph.barriers.empty())
    {
        std::cout << AK::ToDebugString(graph.barriers.front()) << '\n';
        std::cout << AK::ToDebugString(graph.barriers.back()) << '\n';
    }
    if (!graph.lifetimes.empty())
    {
        std::cout << AK::ToDebugString(graph.lifetimes.front()) << '\n';
    }
    if (!graph.aliasGroups.empty())
    {
        std::cout << AK::ToDebugString(graph.aliasGroups.front()) << '\n';
    }
    if (!graph.queueBatches.empty())
    {
        std::cout << AK::ToDebugString(graph.queueBatches.front()) << '\n';
        std::cout << AK::ToDebugString(graph.queueBatches.back()) << '\n';
    }
    std::cout << AK::ToDebugString(probe) << '\n';

    return probe.ok ? 0 : 1;
}
