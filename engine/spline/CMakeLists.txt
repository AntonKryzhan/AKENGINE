add_library(ak_spline STATIC
    src/Spline.cpp
)

add_library(AK::Spline ALIAS ak_spline)

target_include_directories(ak_spline
    PUBLIC
        include
)

target_link_libraries(ak_spline
    PUBLIC
        AK::Core
        AK::Math
        AK::WorldTopology
)

set_target_properties(ak_spline PROPERTIES FOLDER "Engine")
