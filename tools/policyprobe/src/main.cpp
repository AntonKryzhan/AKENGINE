#include <AK/Policy/EnginePolicies.hpp>

#include <iostream>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "ak_policyprobe failed: " << message << '\n';
        return 1;
    }
}

int main()
{
    const AK::FutureLimitProbeResult probe = AK::BuildFutureLimitProbe();
    if (!probe.ok)
    {
        std::cerr << probe.summary << '\n';
        std::cerr << AK::ToDebugString(probe.buildReport) << '\n';
        std::cerr << AK::ToDebugString(probe.unitsReport) << '\n';
        std::cerr << AK::ToDebugString(probe.coordinateReport) << '\n';
        std::cerr << AK::ToDebugString(probe.gpuSyncReport) << '\n';
        std::cerr << AK::ToDebugString(probe.safeShaderReport) << '\n';
        std::cerr << AK::ToDebugString(probe.rejectedShaderReport) << '\n';
        return Fail("future limit guard policy failed");
    }

    AK::GpuSynchronizationPolicy badGpuSync{};
    badGpuSync.framesInFlight = 3;
    badGpuSync.deferredReleaseFrames = 1;
    badGpuSync.allowHotFrameReadback = true;
    const AK::PolicyValidationReport badGpuReport = AK::ValidateGpuSynchronizationPolicy(badGpuSync);
    if (badGpuReport.ok)
    {
        return Fail("unsafe GPU synchronization policy was accepted");
    }

    AK::UnitsPolicy badUnits{};
    badUnits.internalAngles = AK::AngleUnit::Degrees;
    const AK::PolicyValidationReport badUnitsReport = AK::ValidateUnitsPolicy(badUnits);
    if (badUnitsReport.ok)
    {
        return Fail("unsafe unit policy was accepted");
    }

    AK::ShaderPermutationPlan runtimePlan = AK::BuildDefaultShaderPermutationPlan();
    runtimePlan.RegisterFeature({"debug_runtime_compile", 2, true, true});
    const AK::ShaderPermutationReport shippingRuntimeReport = runtimePlan.Analyze(probe.policies.shaderBudget, AK::BuildMode::Shipping);
    if (shippingRuntimeReport.withinBudget)
    {
        return Fail("shipping runtime shader compilation risk was accepted");
    }

    std::cout << "[ ok ] " << probe.summary << '\n';
    std::cout << "[ ok ] " << AK::ToDebugString(probe.buildReport) << '\n';
    std::cout << "[ ok ] " << AK::ToDebugString(probe.unitsReport) << '\n';
    std::cout << "[ ok ] " << AK::ToDebugString(probe.coordinateReport) << '\n';
    std::cout << "[ ok ] " << AK::ToDebugString(probe.gpuSyncReport) << '\n';
    std::cout << "[ ok ] " << AK::ToDebugString(probe.safeShaderReport) << '\n';
    std::cout << "[ ok ] rejected " << AK::ToDebugString(probe.rejectedShaderReport) << '\n';
    std::cout << "[ ok ] unsafe gpu sync rejected warnings=" << badGpuReport.warnings << '\n';
    std::cout << "[ ok ] unsafe units rejected warnings=" << badUnitsReport.warnings << '\n';
    std::cout << "[ ok ] shipping runtime shader compile rejected\n";
    return 0;
}
