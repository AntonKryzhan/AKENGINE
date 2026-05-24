#include <AK/Render/MeshOptimization.hpp>
#include <AK/Render/PrimitiveMesh.hpp>
#include <AK/RHI/VulkanRHI.hpp>

#include <iostream>

int main()
{
    const AK::RenderMeshOptimizationProbe probe = AK::BuildRenderMeshOptimizationProbe();
    const auto& stats = probe.stats;
    const auto& config = probe.config;
    const AK::VulkanVertexLayoutDesc packedLayout = AK::BuildPackedVulkanMeshVertexLayout32();
    const bool layoutOk = packedLayout.valid && packedLayout.strideBytes == sizeof(AK::PackedRenderVertex32);

    std::cout << (probe.ok && layoutOk ? "[ ok ]" : "[ fail ]")
              << " render mesh optimization / meshing-main adaptation"
              << " strategy=" << AK::ToString(config.strategy)
              << " packing=" << AK::ToString(config.vertexPacking)
              << " source_vertices=" << stats.sourceVertices
              << " packed_vertices=" << stats.outputVertices
              << " indices=" << stats.outputIndices
              << " draws=" << stats.drawCommands
              << " meshlets=" << stats.meshlets
              << " lods=" << stats.lodRanges
              << " save=" << static_cast<int>(stats.vertexByteSavingsPercent) << "%"
              << " packed_stride=" << packedLayout.strideBytes
              << " packed_attrs=" << packedLayout.attributes.size()
              << " combined=" << (stats.combinedBuffers ? "true" : "false")
              << " finite=" << (stats.finite ? "true" : "false")
              << " warnings=" << stats.validationWarnings
              << '\n';

    std::cout << AK::ToDebugString(probe.sourceStudy) << '\n';
    std::cout << AK::ToDebugString(stats) << '\n';
    std::cout << AK::ToDebugString(probe) << '\n';

    return (probe.ok && layoutOk) ? 0 : 1;
}
