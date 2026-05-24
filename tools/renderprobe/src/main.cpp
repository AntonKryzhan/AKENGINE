#include <AK/Render/RenderFoundation.hpp>

#include <iostream>

int main()
{
    const AK::DepthPolicy policy = AK::MakeDefaultDepthPolicy();
    const AK::DepthPrecisionReport report = AK::AnalyzeDepthPolicy(policy);
    const float nearZ = AK::ProjectViewDepthToDeviceZ(policy.nearPlane, policy);
    const float farZ = AK::ProjectViewDepthToDeviceZ(100000.0f, policy);

    AK::RenderGraphPlan graph = AK::BuildDefaultRenderGraphPlan();
    const AK::RenderGraphStats stats = graph.Build();
    const AK::RenderFoundationProbe probe = AK::BuildRenderFoundationProbe();

    const bool depthOk = report.valid && report.reversedZ && report.floatingPointDepth && nearZ > farZ;
    const bool graphOk = stats.passes == 4 && stats.resources == 4 && stats.presentPasses == 1 && stats.validationWarnings == 0;
    const bool relativeOk = probe.cameraRelative.finite && !probe.cameraRelative.precisionRisk;

    std::cout << "[ ok ] " << AK::ToDebugString(report) << '\n';
    std::cout << "[ ok ] deviceZ near=" << nearZ << " far=" << farZ << '\n';
    std::cout << "[ ok ] render graph " << AK::ToDebugString(stats) << '\n';
    std::cout << "[ ok ] " << probe.summary << '\n';

    return depthOk && graphOk && relativeOk ? 0 : 1;
}
