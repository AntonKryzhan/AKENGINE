add_library(ak_surface STATIC
    src/Surface.cpp
)

add_library(AK::Surface ALIAS ak_surface)

target_include_directories(ak_surface
    PUBLIC
        include
)

target_link_libraries(ak_surface
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
        AK::Terrain
)

set_target_properties(ak_surface PROPERTIES FOLDER "Engine")
