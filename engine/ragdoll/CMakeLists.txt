add_library(ak_ragdoll STATIC
    src/Ragdoll.cpp
)

add_library(AK::Ragdoll ALIAS ak_ragdoll)

target_include_directories(ak_ragdoll
    PUBLIC
        include
)

target_link_libraries(ak_ragdoll
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
)

set_target_properties(ak_ragdoll PROPERTIES FOLDER "Engine")
