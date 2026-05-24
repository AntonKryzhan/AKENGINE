add_library(ak_worldtopology STATIC
    src/WorldTopology.cpp
)

add_library(AK::WorldTopology ALIAS ak_worldtopology)

target_include_directories(ak_worldtopology
    PUBLIC
        include
)

target_link_libraries(ak_worldtopology
    PUBLIC
        ak_core
        ak_math
        ak_world
)

set_target_properties(ak_worldtopology PROPERTIES FOLDER "Engine")
