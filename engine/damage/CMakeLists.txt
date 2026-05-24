add_library(ak_damage STATIC
    src/Damage.cpp
)

add_library(AK::Damage ALIAS ak_damage)

target_include_directories(ak_damage
    PUBLIC
        include
)

target_link_libraries(ak_damage
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
        AK::Projectile
        AK::Debris
        AK::Fluid
        AK::Surface
)

set_target_properties(ak_damage PROPERTIES FOLDER "Engine")
