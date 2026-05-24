#include <AK/ECS/World.hpp>
#include <AK/Visibility/Bounds.hpp>

#include <iostream>

int main()
{
    AK::World world;

    const AK::EntityId cube = world.CreateEntity("Cube");
    AK::TransformComponent& cubeTransform = world.AddTransform(cube);
    cubeTransform.position = {2.0f, 0.0f, 1.0f};
    cubeTransform.scale = {2.0f, 1.0f, 3.0f};
    world.AddMesh(cube);

    const AK::EntityId farCube = world.CreateEntity("FarCube");
    AK::TransformComponent& farTransform = world.AddTransform(farCube);
    farTransform.position = {50.0f, 0.0f, 0.0f};
    world.AddMesh(farCube);

    const AK::BoundsUpdateStats boundsStats = AK::RebuildSceneBounds(world, true);
    const AK::Frustum3 frustum = AK::MakeOrthographicFrustum(-10.0f, 10.0f, -10.0f, 10.0f, -10.0f, 10.0f);
    const AK::VisibilityStats visibilityStats = AK::UpdateFrustumVisibility(world, frustum);
    const std::vector<AK::EntityId> visible = AK::CollectVisibleEntities(world);

    std::cout << "[ ok ] bounds " << AK::ToDebugString(boundsStats) << '\n';
    std::cout << "[ ok ] visibility " << AK::ToDebugString(visibilityStats) << '\n';
    std::cout << "[ ok ] visible count " << visible.size() << '\n';
    std::cout << "[ ok ] " << AK::BuildBoundsVisibilityProbeSummary() << '\n';

    return boundsStats.invalid == 0 && visibilityStats.visible == 1 && visibilityStats.culled == 1 ? 0 : 1;
}
