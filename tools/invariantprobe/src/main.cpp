#include <AK/Assets/AssetRegistry.hpp>
#include <AK/Core/Guid.hpp>
#include <AK/Core/Handle.hpp>
#include <AK/Core/Path.hpp>
#include <AK/Core/Result.hpp>
#include <AK/ECS/World.hpp>
#include <AK/Scene/Scene.hpp>

#include <filesystem>
#include <iostream>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "ak_invariantprobe failed: " << message << '\n';
        return 1;
    }
}

int main()
{
    AK::World world;
    const AK::EntityId first = world.CreateEntity("First");
    world.AddTransform(first);

    if (!world.IsAlive(first))
    {
        return Fail("created entity is not alive");
    }

    if (!world.DestroyEntity(first))
    {
        return Fail("destroy entity failed");
    }

    if (world.IsAlive(first))
    {
        return Fail("stale generational entity handle is still alive");
    }

    const AK::EntityId second = world.CreateEntity("Second");
    if (second.index != first.index || second.generation == first.generation)
    {
        return Fail("entity id generation was not advanced on reuse");
    }

    const AK::AssetGuid guidA = AK::BuildAssetGuidFromPath("Models/../Models/Cube.GLB");
    const AK::AssetGuid guidB = AK::BuildAssetGuidFromNormalizedPath("models/cube.glb");
    if (guidA != guidB || !guidA.IsValid())
    {
        return Fail("normalized asset guid mismatch");
    }

    const std::string guidText = AK::ToString(guidA);
    if (AK::AssetGuidFromString(guidText) != guidA)
    {
        return Fail("asset guid roundtrip failed");
    }

    const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "ak_invariantprobe_scene.akscene";
    AK::Scene scene("InvariantProbe");
    const AK::EntityId entity = scene.GetWorld().CreateEntity("RoundtripEntity");
    scene.GetWorld().AddTransform(entity).position = {1.0f, 2.0f, 3.0f};
    if (!scene.SaveToFile(tempPath))
    {
        return Fail("atomic scene save failed");
    }

    AK::Scene loaded("Empty");
    if (!loaded.LoadFromFile(tempPath) || loaded.GetWorld().Entities().size() != 1)
    {
        return Fail("scene load after atomic save failed");
    }

    std::filesystem::remove(tempPath);

    const AK::Result<int> result = AK::Ok(42);
    if (!result || result.Value() != 42)
    {
        return Fail("Result<T> invariant failed");
    }

    std::cout << "AK invariant probe OK\n";
    std::cout << "stale handle: " << AK::ToString(first) << " -> alive=" << world.IsAlive(first) << '\n';
    std::cout << "reused handle: " << AK::ToString(second) << '\n';
    std::cout << "asset guid: " << guidText << '\n';
    return 0;
}
