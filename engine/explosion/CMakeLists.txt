add_library(ak_explosion STATIC
    src/Explosion.cpp
)

add_library(AK::Explosion ALIAS ak_explosion)

target_include_directories(ak_explosion
    PUBLIC
        include
)

target_link_libraries(ak_explosion
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
        AK::Damage
        AK::Debris
)

set_target_properties(ak_explosion PROPERTIES FOLDER "Engine")
