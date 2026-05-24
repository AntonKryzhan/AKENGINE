add_executable(ak_vulkanmeshprobe
    src/main.cpp
)

target_link_libraries(ak_vulkanmeshprobe
    PRIVATE
        AK::Core
        AK::Platform
        AK::RHI
        AK::Render
)

set_target_properties(ak_vulkanmeshprobe PROPERTIES FOLDER "Tools")
