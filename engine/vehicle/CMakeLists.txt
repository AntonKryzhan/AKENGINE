add_library(ak_vehicle STATIC
    src/Vehicle.cpp
)

add_library(AK::Vehicle ALIAS ak_vehicle)

target_include_directories(ak_vehicle
    PUBLIC
        include
)

target_link_libraries(ak_vehicle
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
)

set_target_properties(ak_vehicle PROPERTIES FOLDER "Engine")
