add_library(ak_terrain STATIC
    src/Terrain.cpp
)

add_library(AK::Terrain ALIAS ak_terrain)

target_include_directories(ak_terrain
    PUBLIC
        include
)

target_link_libraries(ak_terrain
    PUBLIC
        AK::Core
        AK::Math
        AK::WorldTopology
)

set_target_properties(ak_terrain PROPERTIES FOLDER "Engine")
