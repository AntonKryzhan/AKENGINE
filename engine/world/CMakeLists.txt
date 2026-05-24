add_library(ak_world STATIC
    src/WorldCoordinates.cpp
)

add_library(AK::World ALIAS ak_world)

target_include_directories(ak_world
    PUBLIC
        include
)

target_link_libraries(ak_world
    PUBLIC
        ak_core
        ak_math
)

set_target_properties(ak_world PROPERTIES FOLDER "Engine")
