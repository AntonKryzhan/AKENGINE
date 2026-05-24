add_library(ak_debugdraw STATIC
    src/DebugDraw.cpp
)

add_library(AK::DebugDraw ALIAS ak_debugdraw)

target_include_directories(ak_debugdraw
    PUBLIC
        include
)

target_link_libraries(ak_debugdraw
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
        AK::Projectile
        AK::Fluid
        AK::Debris
        AK::Damage
        AK::Explosion
        AK::ForceField
)

set_target_properties(ak_debugdraw PROPERTIES FOLDER "Engine")
