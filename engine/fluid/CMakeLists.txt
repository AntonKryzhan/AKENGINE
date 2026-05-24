add_library(ak_fluid STATIC
    src/Fluid.cpp
)

add_library(AK::Fluid ALIAS ak_fluid)

target_include_directories(ak_fluid
    PUBLIC
        include
)

target_link_libraries(ak_fluid
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
)

set_target_properties(ak_fluid PROPERTIES FOLDER "Engine")
