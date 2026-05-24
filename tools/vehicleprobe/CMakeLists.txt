add_executable(ak_vehicleprobe
    src/main.cpp
)

target_link_libraries(ak_vehicleprobe
    PRIVATE
        AK::Core
        AK::Vehicle
)

set_target_properties(ak_vehicleprobe PROPERTIES FOLDER "Tools")
