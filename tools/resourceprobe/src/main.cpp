#include <AK/Resources/ResourceManager.hpp>

#include <iostream>

int main()
{
    AK::ResourceManager manager({2});

    const AK::ResourceHandle mesh = manager.Create({AK::ResourceType::Mesh, AK::BuildAssetGuidFromNormalizedPath("builtin:cube"), "builtin:cube", 65536}, 100);
    const AK::ResourceHandle texture = manager.Create({AK::ResourceType::Texture, AK::BuildAssetGuidFromNormalizedPath("builtin:white"), "builtin:white", 4096}, 100);

    const bool createdOk = manager.IsAlive(mesh) && manager.IsAlive(texture) && manager.Stats().alive == 2;
    const bool destroyRequested = manager.RequestDestroy(mesh, 101);
    const bool pendingOk = manager.IsResident(mesh) && !manager.IsAlive(mesh) && manager.Stats().pendingDestroy == 1;
    const AK::u32 earlyRelease = manager.ProcessDeferredReleases(102);
    const AK::u32 finalRelease = manager.ProcessDeferredReleases(103);
    const bool staleRejected = !manager.IsResident(mesh) && !manager.IsAlive(mesh);

    const AK::ResourceHandle material = manager.Create({AK::ResourceType::Material, AK::BuildAssetGuidFromNormalizedPath("builtin:default"), "builtin:default", 2048}, 104);
    const bool generationOk = material.index == mesh.index && material.generation != mesh.generation;
    const bool statsOk = manager.Stats().alive == 2 && manager.Stats().totalReleased == 1;

    std::cout << "[ ok ] created " << AK::ToDebugString(manager.Stats()) << '\n';
    std::cout << "[ ok ] stale handle rejected " << (staleRejected ? "yes" : "no") << '\n';
    std::cout << "[ ok ] reused handle " << AK::ToString(material) << '\n';
    std::cout << "[ ok ] " << AK::BuildResourceProbeSummary() << '\n';

    return createdOk && destroyRequested && pendingOk && earlyRelease == 0 && finalRelease == 1 && staleRejected && generationOk && statsOk ? 0 : 1;
}
