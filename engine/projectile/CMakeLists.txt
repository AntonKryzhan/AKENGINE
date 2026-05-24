add_library(ak_projectile STATIC
    src/Projectile.cpp
)

add_library(AK::Projectile ALIAS ak_projectile)

target_include_directories(ak_projectile
    PUBLIC
        include
)

target_link_libraries(ak_projectile
    PUBLIC
        AK::Core
        AK::Math
        AK::CSG
        AK::World
        AK::WorldTopology
        AK::Gravity
        AK::Physics
        AK::Surface
)

set_target_properties(ak_projectile PROPERTIES FOLDER "Engine")
