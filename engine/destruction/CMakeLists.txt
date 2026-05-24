add_library(ak_destruction STATIC
    src/Destruction.cpp
)

add_library(AK::Destruction ALIAS ak_destruction)

target_include_directories(ak_destruction
    PUBLIC
        include
)

target_link_libraries(ak_destruction
    PUBLIC
        AK::Core
        AK::Math
        AK::CSG
        AK::Physics
        AK::ECS
        AK::Visibility
        AK::Resources
        AK::WorldPartition
        AK::Diagnostics
)

set_target_properties(ak_destruction PROPERTIES FOLDER "Engine")
