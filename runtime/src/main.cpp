#include <AK/Core/Log.hpp>
#include <AK/Platform/Window.hpp>
#include <AK/Render/Renderer.hpp>
#include <AK/Scene/Scene.hpp>

#include <chrono>
#include <filesystem>
#include <thread>

int main(int argc, char** argv)
{
    const std::filesystem::path scenePath = argc > 1 ? argv[1] : "projects/Sandbox/scene.akscene";

    AK::LogInfo("AK Player starting");

    AK::Scene scene("RuntimeScene");
    if (std::filesystem::exists(scenePath))
    {
        scene.LoadFromFile(scenePath);
    }
    else
    {
        const AK::EntityId entity = scene.GetWorld().CreateEntity("RuntimeEntity");
        scene.GetWorld().AddTransform(entity);
    }

    AK::Window window({"AK Player v0.1", 1280, 720});
    if (!window.IsOpen())
    {
        AK::LogError("Player window failed to open");
        return 1;
    }

    AK::Renderer renderer;
    renderer.Initialize({"AK Player"}, window);

    while (window.PollEvents())
    {
        renderer.BeginFrame();
        renderer.EndFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    renderer.Shutdown();
    AK::LogInfo("AK Player stopped");
    return 0;
}
