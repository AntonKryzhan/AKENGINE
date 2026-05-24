#include <AK/Render/VulkanRuntimeArchitecture.hpp>

#include <iostream>

int main()
{
    const AK::VulkanRuntimeArchitectureProbe probe = AK::BuildVulkanRuntimeArchitectureProbe();

    std::cout << (probe.ok ? "[ ok ] " : "[fail] ")
              << "vulkan runtime architecture hardening"
              << " features=" << probe.plan.features.size()
              << " allocators=" << probe.plan.memoryAllocators.size()
              << " descriptor_sets=" << probe.plan.descriptorSets.size()
              << " command_pools=" << probe.plan.commandPools.size()
              << " submit_batches=" << probe.plan.submitBatches.size()
              << " bind_filter=" << probe.plan.bindFilter.outputCommands << "/" << probe.plan.bindFilter.inputCommands
              << " pipeline_cache=" << (probe.plan.pipelineCache.valid ? "ready" : "no")
              << " async_compile=" << (probe.plan.compileQueue.valid ? "ready" : "no")
              << " ready_handles=" << (probe.plan.readyForRealVkHandles ? "true" : "false")
              << " warnings=" << probe.plan.warnings
              << '\n';

    std::cout << AK::ToDebugString(probe.plan) << '\n';
    std::cout << AK::ToDebugString(probe.plan.limits) << '\n';
    std::cout << AK::ToDebugString(probe.plan.pipelineCache) << '\n';
    std::cout << AK::ToDebugString(probe.plan.shaderVariant) << '\n';
    std::cout << AK::ToDebugString(probe.plan.compileQueue) << '\n';
    std::cout << AK::ToDebugString(probe.plan.bindFilter) << '\n';

    if (!probe.plan.features.empty())
    {
        std::cout << AK::ToDebugString(probe.plan.features.front()) << '\n';
    }
    if (!probe.plan.memoryAllocators.empty())
    {
        std::cout << AK::ToDebugString(probe.plan.memoryAllocators.front()) << '\n';
    }
    if (!probe.plan.descriptorSets.empty())
    {
        std::cout << AK::ToDebugString(probe.plan.descriptorSets.front()) << '\n';
    }
    if (!probe.plan.commandPools.empty())
    {
        std::cout << AK::ToDebugString(probe.plan.commandPools.front()) << '\n';
    }
    if (!probe.plan.submitBatches.empty())
    {
        std::cout << AK::ToDebugString(probe.plan.submitBatches.back()) << '\n';
    }

    return probe.ok ? 0 : 1;
}
