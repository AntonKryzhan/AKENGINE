add_library(ak_worldpartition
    include/AK/WorldPartition/WorldPartition.hpp
    src/WorldPartition.cpp
)

add_library(AK::WorldPartition ALIAS ak_worldpartition)

target_include_directories(ak_worldpartition
    PUBLIC
        include
)

target_link_libraries(ak_worldpartition
    PUBLIC
        AK::Core
        AK::World
)

set_target_properties(ak_worldpartition PROPERTIES FOLDER "Engine")
