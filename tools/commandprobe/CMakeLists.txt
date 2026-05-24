add_executable(ak_commandprobe
    src/main.cpp
)

target_link_libraries(ak_commandprobe
    PRIVATE
        AK::Core
        AK::Commands
)

set_target_properties(ak_commandprobe PROPERTIES FOLDER "Tools")
