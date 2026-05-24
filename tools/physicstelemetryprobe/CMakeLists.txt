add_executable(ak_physicstelemetryprobe
    src/main.cpp
)

target_link_libraries(ak_physicstelemetryprobe
    PRIVATE
        AK::PhysicsTelemetry
)

set_target_properties(ak_physicstelemetryprobe PROPERTIES FOLDER "Tools")
