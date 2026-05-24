add_library(ak_physicstelemetry STATIC
    src/PhysicsTelemetry.cpp
)

add_library(AK::PhysicsTelemetry ALIAS ak_physicstelemetry)

target_include_directories(ak_physicstelemetry
    PUBLIC
        include
)

target_link_libraries(ak_physicstelemetry
    PUBLIC
        AK::Core
        AK::Math
        AK::Physics
)

set_target_properties(ak_physicstelemetry PROPERTIES FOLDER "Engine")
