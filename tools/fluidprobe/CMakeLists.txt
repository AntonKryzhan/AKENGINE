add_executable(ak_fluidprobe
    src/main.cpp
)

target_link_libraries(ak_fluidprobe
    PRIVATE
        AK::Core
        AK::Fluid
)

set_target_properties(ak_fluidprobe PROPERTIES FOLDER "Tools")
