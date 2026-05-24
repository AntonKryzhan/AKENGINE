add_executable(ak_gravityprobe
    src/main.cpp
)

target_link_libraries(ak_gravityprobe
    PRIVATE
        AK::Core
        AK::Gravity
)

set_target_properties(ak_gravityprobe PROPERTIES FOLDER "Tools")
