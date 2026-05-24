#include <AK/NTC/NeuralTextureCompression.hpp>
#include <AK/Texture/TextureSet.hpp>

#include <iostream>

int main()
{
    const AK::NtcProbeResult probe = AK::BuildNtcProbe();
    std::cout << probe.summary << '\n';
    std::cout << probe.textureProbe.summary << '\n';
    std::cout << AK::ToDebugString(probe.currentPlan) << '\n';
    std::cout << AK::ToDebugString(probe.futureVulkanPlan) << '\n';

    if (!probe.currentPlan.warnings.empty())
    {
        std::cout << "current warnings:" << '\n';
        for (const std::string& warning : probe.currentPlan.warnings)
        {
            std::cout << "  - " << warning << '\n';
        }
    }

    return probe.ok ? 0 : 1;
}
