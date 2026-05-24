add_library(ak_forcefield STATIC
    src/ForceField.cpp
)

add_library(AK::ForceField ALIAS ak_forcefield)

target_include_directories(ak_forcefield
    PUBLIC
        include
)

target_link_libraries(ak_forcefield
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
        AK::Damage
)

set_target_properties(ak_forcefield PROPERTIES FOLDER "Engine")
