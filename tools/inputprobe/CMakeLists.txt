add_executable(ak_inputprobe
    src/main.cpp
)

target_link_libraries(ak_inputprobe
    PRIVATE
        AK::Core
        AK::Platform
        AK::Input
)

set_target_properties(ak_inputprobe PROPERTIES FOLDER "Tools")
