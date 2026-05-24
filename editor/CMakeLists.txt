add_executable(ak_editor
    src/main.cpp
)

target_link_libraries(ak_editor
    PRIVATE
        AK::Core
        AK::Filesystem
        AK::Settings
        AK::Serialization
        AK::Platform
        AK::ECS
        AK::Scene
        AK::Math
        AK::Assets
        AK::Package
        AK::VFS
        AK::Streaming
        AK::Resources
        AK::Memory
        AK::Diagnostics
        AK::Jobs
        AK::Render
        AK::Visibility
        AK::WorldPartition
        AK::WorldTopology
        AK::Gravity
        AK::Terrain
        AK::Spline
        AK::CSG
        AK::Texture
        AK::NTC
        AK::Input
        AK::Commands
        AK::EditorUI
)

set_target_properties(ak_editor PROPERTIES FOLDER "Applications")
