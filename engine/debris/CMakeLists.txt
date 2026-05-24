add_library(ak_debris STATIC
    src/Debris.cpp
)

add_library(AK::Debris ALIAS ak_debris)

target_include_directories(ak_debris
    PUBLIC
        include
)

target_link_libraries(ak_debris
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
        AK::Projectile
        AK::Fluid
        AK::Surface
        AK::CSG
)

set_target_properties(ak_debris PROPERTIES FOLDER "Engine")
