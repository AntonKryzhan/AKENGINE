add_library(ak_csg STATIC
    src/Boolean.cpp
)

add_library(AK::CSG ALIAS ak_csg)

target_include_directories(ak_csg
    PUBLIC
        include
)

target_link_libraries(ak_csg
    PUBLIC
        AK::Core
        AK::Math
)

set_target_properties(ak_csg PROPERTIES FOLDER "Engine")
