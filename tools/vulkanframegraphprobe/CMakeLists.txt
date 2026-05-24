add_executable(ak_vulkanframegraphprobe
    src/main.cpp
)

target_link_libraries(ak_vulkanframegraphprobe
    PRIVATE
        AK::Render
)

set_target_properties(ak_vulkanframegraphprobe PROPERTIES FOLDER "Tools")
