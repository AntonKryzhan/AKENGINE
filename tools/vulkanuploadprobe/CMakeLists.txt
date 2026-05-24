add_executable(ak_vulkanuploadprobe
    src/main.cpp
)

target_link_libraries(ak_vulkanuploadprobe
    PRIVATE
        AK::Core
        AK::Math
        AK::Platform
        AK::RHI
        AK::Render
)

set_target_properties(ak_vulkanuploadprobe PROPERTIES FOLDER "Tools")
