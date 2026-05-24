add_library(ak_remesh STATIC
    src/Remesh.cpp
)

add_library(AK::Remesh ALIAS ak_remesh)

target_include_directories(ak_remesh
    PUBLIC
        include
)

target_link_libraries(ak_remesh
    PUBLIC
        AK::Core
        AK::Math
        AK::CSG
)

set_target_properties(ak_remesh PROPERTIES FOLDER "Engine")
