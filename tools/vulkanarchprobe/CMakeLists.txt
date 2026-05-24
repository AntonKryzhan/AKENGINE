add_executable(ak_vulkanarchprobe
    src/main.cpp
)

target_link_libraries(ak_vulkanarchprobe
    PRIVATE
        AK::Render
)

set_target_properties(ak_vulkanarchprobe PROPERTIES FOLDER "Tools")
