add_executable(ak_clockprobe
    src/main.cpp
)

target_link_libraries(ak_clockprobe
    PRIVATE
        AK::Core
)

set_target_properties(ak_clockprobe PROPERTIES FOLDER "Tools")
