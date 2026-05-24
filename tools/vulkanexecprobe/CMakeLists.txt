add_executable(ak_vulkanexecprobe
    src/main.cpp
)

target_link_libraries(ak_vulkanexecprobe
    PRIVATE
        AK::Render
)

set_target_properties(ak_vulkanexecprobe PROPERTIES FOLDER "Tools")
