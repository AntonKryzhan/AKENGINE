add_library(ak_gravity STATIC
    src/GravityField.cpp
)

add_library(AK::Gravity ALIAS ak_gravity)

target_include_directories(ak_gravity
    PUBLIC
        include
)

target_link_libraries(ak_gravity
    PUBLIC
        AK::Core
        AK::Math
        AK::World
        AK::WorldTopology
)

set_target_properties(ak_gravity PROPERTIES FOLDER "Engine")
