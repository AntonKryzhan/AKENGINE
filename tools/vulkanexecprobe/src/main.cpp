#include <AK/Render/VulkanFrameGraphExecution.hpp>

#include <iostream>

int main()
{
    const AK::VulkanFrameGraphExecutionProbe probe = AK::BuildVulkanFrameGraphExecutionProbe();
    const AK::VulkanFrameGraphExecutionPlan& plan = probe.execution;

    std::cout << (probe.ok ? "[ ok ] " : "[fail] ")
              << "vulkan framegraph execution / command recording foundation"
              << " passes=" << plan.passes.size()
              << " commands=" << plan.commands.size()
              << " cmdbufs=" << plan.stats.commandBuffers
              << " barriers=" << plan.stats.barrierCommands
              << " copies=" << plan.stats.copyCommands
              << " draws=" << plan.stats.drawCommands
              << " dispatches=" << plan.stats.dispatchCommands
              << " submits=" << plan.submits.size()
              << " transient=" << plan.stats.transientHeapBytes
              << " saved=" << plan.stats.savedTransientBytes
              << " warnings=" << plan.validation.warnings
              << " ready=" << (plan.readyForPersistentRenderer ? "true" : "false")
              << '\n';

    std::cout << AK::ToDebugString(plan) << '\n';
    std::cout << AK::ToDebugString(plan.config) << '\n';
    std::cout << AK::ToDebugString(plan.stats) << '\n';
    std::cout << AK::ToDebugString(plan.validation) << '\n';
    if (!plan.frameResources.empty())
    {
        std::cout << AK::ToDebugString(plan.frameResources.front()) << '\n';
    }
    if (!plan.passes.empty())
    {
        std::cout << AK::ToDebugString(plan.passes.front()) << '\n';
        std::cout << AK::ToDebugString(plan.passes.back()) << '\n';
    }
    if (!plan.commands.empty())
    {
        std::cout << AK::ToDebugString(plan.commands.front()) << '\n';
        if (plan.commands.size() > 1)
        {
            std::cout << AK::ToDebugString(plan.commands[1]) << '\n';
        }
        std::cout << AK::ToDebugString(plan.commands.back()) << '\n';
    }
    if (!plan.submits.empty())
    {
        std::cout << AK::ToDebugString(plan.submits.front()) << '\n';
        std::cout << AK::ToDebugString(plan.submits.back()) << '\n';
    }
    std::cout << AK::ToDebugString(probe) << '\n';

    return probe.ok ? 0 : 1;
}
