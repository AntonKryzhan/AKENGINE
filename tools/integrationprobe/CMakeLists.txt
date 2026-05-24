add_executable(ak_integrationprobe
    src/main.cpp
)

target_link_libraries(ak_integrationprobe
    PRIVATE
        AK::Core
        AK::Integration
        AK::RHI
        AK::Render
)

set_target_properties(ak_integrationprobe PROPERTIES FOLDER "Tools")
