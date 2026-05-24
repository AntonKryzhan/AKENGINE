#include <AK/Scene/Scene.hpp>
#include <AK/World/WorldCoordinates.hpp>

#include <filesystem>
#include <iostream>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "[fail] " << message << '\n';
            return false;
        }

        std::cout << "[ ok ] " << message << '\n';
        return true;
    }
}

int main()
{
    AK::Scene scene("ECSProbe");

    const AK::EntityId meshEntity = scene.GetWorld().CreateEntity("ProbeMesh");
    scene.GetWorld().AddTransform(meshEntity).position = {1.0f, 2.0f, 3.0f};
    scene.GetWorld().AddWorldPosition(meshEntity).position = AK::MakeWorldPosition(1000000.0, 2.0, 3.0);
    AK::MeshComponent& mesh = scene.GetWorld().AddMesh(meshEntity);
    mesh.mesh = "builtin:cube";
    mesh.material = "builtin:default";
    scene.GetWorld().AddBounds(meshEntity).localBounds = AK::MakeAABB3FromCenterExtents({0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f});

    const AK::EntityId cameraEntity = scene.GetWorld().CreateEntity("ProbeCamera");
    scene.GetWorld().AddTransform(cameraEntity).position = {0.0f, 1.5f, -5.0f};
    scene.GetWorld().AddWorldPosition(cameraEntity).position = AK::MakeWorldPosition(0.0, 1.5, -5.0);
    AK::CameraComponent& camera = scene.GetWorld().AddCamera(cameraEntity);
    camera.primary = true;

    const AK::EntityId lightEntity = scene.GetWorld().CreateEntity("ProbeLight");
    scene.GetWorld().AddTransform(lightEntity).position = {2.0f, 4.0f, -2.0f};
    scene.GetWorld().AddWorldPosition(lightEntity).position = AK::MakeWorldPosition(2.0, 4.0, -2.0);
    AK::LightComponent& light = scene.GetWorld().AddLight(lightEntity);
    light.type = AK::LightType::Directional;
    light.intensity = 3.0f;

    bool ok = true;
    ok &= Check(scene.GetWorld().Entities().size() == 3, "created 3 entities");
    ok &= Check(scene.GetWorld().TransformCount() == 3, "3 transform components");
    ok &= Check(scene.GetWorld().WorldPositionCount() == 3, "3 world position components");
    ok &= Check(scene.GetWorld().BoundsCount() == 1, "1 bounds component");
    ok &= Check(scene.GetWorld().MeshCount() == 1, "1 mesh component");
    ok &= Check(scene.GetWorld().CameraCount() == 1, "1 camera component");
    ok &= Check(scene.GetWorld().LightCount() == 1, "1 light component");

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "ak_ecsprobe_scene.akscene";
    ok &= Check(scene.SaveToFile(path), "saved AKSCENE 6 file");

    AK::Scene loaded;
    ok &= Check(loaded.LoadFromFile(path), "loaded AKSCENE 6 file");
    ok &= Check(loaded.GetWorld().Entities().size() == 3, "roundtrip entity count");
    ok &= Check(loaded.GetWorld().WorldPositionCount() == 3, "roundtrip world position count");
    ok &= Check(loaded.GetWorld().BoundsCount() == 1, "roundtrip bounds count");
    ok &= Check(loaded.GetWorld().MeshCount() == 1, "roundtrip mesh count");
    ok &= Check(loaded.GetWorld().CameraCount() == 1, "roundtrip camera count");
    ok &= Check(loaded.GetWorld().LightCount() == 1, "roundtrip light count");

    std::error_code error;
    std::filesystem::remove(path, error);

    if (!ok)
    {
        std::cerr << "AK ECS probe failed\n";
        return 1;
    }

    std::cout << "AK ECS probe passed\n";
    return 0;
}
