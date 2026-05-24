add_executable(ak_vulkanprobe
    src/main.cpp
)

target_link_libraries(ak_vulkanprobe
    PRIVATE
        AK::Core
        AK::Platform
        AK::RHI
        AK::Render
)

set_target_properties(ak_vulkanprobe PROPERTIES FOLDER "Tools")
