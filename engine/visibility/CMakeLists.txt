add_library(ak_visibility STATIC
    src/Bounds.cpp
)

add_library(AK::Visibility ALIAS ak_visibility)

target_include_directories(ak_visibility
    PUBLIC
        include
)

target_link_libraries(ak_visibility
    PUBLIC
        AK::Core
        AK::Math
        AK::ECS
)

set_target_properties(ak_visibility PROPERTIES FOLDER "Engine")
