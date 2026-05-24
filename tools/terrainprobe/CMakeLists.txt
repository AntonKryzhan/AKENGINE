add_executable(ak_terrainprobe
    src/main.cpp
)

target_link_libraries(ak_terrainprobe
    PRIVATE
        AK::Core
        AK::Terrain
)

set_target_properties(ak_terrainprobe PROPERTIES FOLDER "Tools")
