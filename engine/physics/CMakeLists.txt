add_library(ak_physics STATIC
    src/Physics.cpp
)

add_library(AK::Physics ALIAS ak_physics)

target_include_directories(ak_physics
    PUBLIC
        include
)

target_link_libraries(ak_physics
    PUBLIC
        AK::Core
        AK::Math
        AK::CSG
)

set_target_properties(ak_physics PROPERTIES FOLDER "Engine")
